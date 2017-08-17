#pragma once
#include <list>
#include <mutex>
#include <thread>
#include "SoundOut.h"
#include "DrawVideo.h"
struct AVPacket;
struct AVFormatContext;
struct AVStream;
struct AVIOContext;
struct AVFrame;
struct SwrContext;
struct SwsContext;
#define MAX_AUDIO_FRAME_SIZE 192000
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0
#define SAMPLE_CORRECTION_PERCENT_MAX 10
#define AUDIO_DIFF_AVG_NB 20
#define SDL_AUDIO_BUFFER_SIZE 1024

enum {
  AV_SYNC_AUDIO_MASTER,
  AV_SYNC_VIDEO_MASTER,
  AV_SYNC_EXTERNAL_MASTER,
};
#define DEFAULT_AV_SYNC_TYPE AV_SYNC_AUDIO_MASTER
#define VIDEO_PICTURE_QUEUE_SIZE 1

struct PacketQueue {
  std::list<AVPacket*> list;
  int size; // packet bytes size
  std::mutex mutex;
  std::condition_variable cond;
  PacketQueue():size(0) {}
  int packets() { return list.size(); }
  int put(AVPacket *pkt);
  int get(AVPacket* &pkt, int block);

  void clear() {
    std::unique_lock<std::mutex> l(mutex);
    list.clear();
    size = 0;
  }
};

struct VideoPicture {
  VideoPicture() { bmp = NULL; width = height = 0; pts = 0; }
  ~VideoPicture() { Clear(); }
  void Clear();
  uint8_t* GetBuffer(int w, int h);
  uint8_t *bmp;
  int width, height; /* source height & width */
  double pts;
};

class FFEvent {
public:
  virtual bool onAudioStream(int steam_id, int codec, int samplerate, int channel) = 0;
  virtual bool onVideoStream(int steam_id, int codec, int width, int height) = 0;
  virtual bool onVideoFrame(VideoPicture* pic) = 0;
};

class FFPlayer
{
  AVFormatContext *pFormatCtx;
  int             videoStream, audioStream; // stream index

  int             av_sync_type;
  double          external_clock; /* external clock base */
  int64_t         external_clock_time;
  // for seek
  int             seek_req;
  int             seek_flags;
  int64_t         seek_pos;

  double          audio_clock;
  AVStream        *audio_st;
  PacketQueue     audioq; // audio packet queue
  // audio pcm buffer(buffer for SDL_Audio output)
  uint8_t         audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
  unsigned int    audio_buf_size;
  unsigned int    audio_buf_index;
  // audio packet for decode
  AVPacket        *audio_pkt;
  uint8_t         *audio_pkt_data;
  int             audio_pkt_size;

  double          audio_diff_cum; /* used for AV difference average computation */
  double          audio_diff_avg_coef;
  double          audio_diff_threshold;
  int             audio_diff_avg_count;

  double          frame_timer;
  double          frame_last_pts;
  double          frame_last_delay;
  double          video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame
  double          video_current_pts; ///<current displayed pts (different from video_clock if frame fifos are used)
  int64_t         video_current_pts_time;  ///<time (av_gettime) at which we updated video_current_pts - used to have running video pts
  AVStream        *video_st;
  // video packet queue
  PacketQueue     videoq;
  VideoPicture    pictq[VIDEO_PICTURE_QUEUE_SIZE];
  int             pictq_size, pictq_rindex, pictq_windex;
  std::mutex      pictq_mutex;
  std::condition_variable pictq_cond;

  std::thread      parse_tid; // demuxer thread
  std::thread      video_tid; // video decord thread
  std::thread      render_tid;
  char             filename[1024];
  volatile int     quit;

  AVIOContext     *io_context;
  SwrContext      *swr_audio;
  SwsContext      *sws_video;
  /* Add or subtract samples to get a better sync, return new
  audio buffer size */
  int synchronize_audio(short *samples,
    int samples_size, double pts);

  int audio_decode_frame(double *pts_ptr);
  int decode_frame_from_packet(AVFrame& decoded_frame);

  int demuxer_thread_func();
  int stream_component_open(int stream_index);

  void video_render_func();
  int video_decode_func();

  double synchronize_video(AVFrame *src_frame, double pts);
  int queue_picture(AVFrame *pFrame, double pts);
  void display_video(VideoPicture * vp);

  FFEvent* event_;
  volatile bool muted_, paused_;
public:
  void set_event(FFEvent* e) { event_ = e; }
  FFEvent* event(FFEvent* e) { return event_; }
  bool muted() const { return muted_; }
  bool set_muted(bool mute) { muted_ = mute; }
  bool paused() const { return paused_; }
  bool pause() { paused_ = true; }
  bool resume() { paused_ = false; }
  double duration();
  void getAudioFrame(unsigned char *stream, int len);

  FFPlayer();
  ~FFPlayer();

  bool Open(const char* path);
  bool isOpen() const { return !quit; }
  bool Close();

  // 获取当前音频播放时间(浮点型的s) audio_clock - audio_buffer_before_play
  double get_audio_clock();
  double get_video_clock();
  double get_external_clock();
  double get_master_clock() {
    switch (av_sync_type)
    {
    case AV_SYNC_VIDEO_MASTER:
      return get_video_clock();
    case AV_SYNC_AUDIO_MASTER:
      return get_audio_clock();
    default:
      return get_external_clock();
      break;
    }
  }

  void seek(double pos, int rel);
};

