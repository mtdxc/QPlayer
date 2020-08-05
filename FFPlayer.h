#pragma once
#include <list>
#include <mutex>
#include <thread>
#include <condition_variable>
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
  ~PacketQueue(){ clear(); }
  std::list<AVPacket*> list;
  int size; // packet bytes size
  std::mutex mutex;
  std::condition_variable cond;
  PacketQueue():size(0) {}
  int packets() { return list.size(); }
  int put(AVPacket *pkt);
  AVPacket* get(bool block);
  void clear();
};

// 视频帧
struct VideoPicture {
  VideoPicture() { clear(); }
  ~VideoPicture() { clear(); }
  void clear();
  uint8_t* GetBuffer(int w, int h);
  uint8_t *bmp = nullptr;
  int width, height; /* source height & width */
  double pts;
};

// FFPlayer事件
class FFEvent {
public:
  // 音频流回调，返回false则不处理音频
  virtual bool onAudioStream(int steam_id, int codec, int samplerate, int channel) = 0;
  // 视频流回调，返回false则不处理视频，也不会有onVideoFrame回调
  virtual bool onVideoStream(int steam_id, int codec, int width, int height) = 0;
  // 视频帧(渲染)回调
  virtual void onVideoFrame(VideoPicture* pic) = 0;
  virtual void onSeekDone(float pos, int code){}
};

/**
播放器类.

采用FFMpeg实现播放，能播放基本所有格式.
音频采用主动getAudioFrame获取方式渲染；
而视频采用回调方式FFEvent::onVideoFrame渲染；
*/
class FFPlayer
{
  AVFormatContext *pFormatCtx;
  int             videoStream, audioStream; // stream index
  char             filename[1024];
  volatile int     quit;

  AVIOContext     *io_context;
  SwrContext      *swr_audio;
  SwsContext      *sws_video;

  int stream_component_open(int stream_index);

  FFEvent* event_;
  volatile bool muted_, paused_;

  // 支持三种类型的音视频同步，详见 AV_SYNC_*
  int             av_sync_type;

  // for seek
  bool            seek_req;
  double          seek_pos;
  int             seek_flags;

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

  /* used for AV difference average computation */
  double          audio_diff_cum; 
  double          audio_diff_avg_coef;
  double          audio_diff_threshold;
  int             audio_diff_avg_count;
  /* Add or subtract samples to get a better sync, return new audio buffer size */
  int synchronize_audio(short *samples, int samples_size, double pts);

  int audio_decode_frame(double *pts_ptr);
  int decode_frame_from_packet(AVFrame& decoded_frame);


  AVStream        *video_st;
  // video packet queue
  PacketQueue     videoq;

  // for video pts
  double synchronize_video(AVFrame *src_frame, double pts);
  double          frame_timer;
  double          frame_last_pts;
  double          frame_last_delay;
  // pts of last decoded frame / predicted pts of next decoded frame
  double          video_clock;

  void display_video(VideoPicture * vp);
  // current displayed pts (different from video_clock if frame fifos are used)
  double          video_current_pts;
  // time (av_gettime) at which we updated video_current_pts - used to have running video pts
  int64_t         video_current_pts_time;

  int queue_picture(AVFrame *pFrame, double pts);
  // rgb video queue
  VideoPicture    pictq[VIDEO_PICTURE_QUEUE_SIZE];
  int             pictq_size, pictq_rindex, pictq_windex;
  std::mutex      pictq_mutex;
  std::condition_variable pictq_cond;

  std::thread      parse_tid; // file demuxer thread
  int demuxer_thread_func();
  std::thread      video_tid; // video decord thread
  int video_decode_func();
  std::thread      render_tid;// video render thread
  void video_render_func();


  // 获取当前音频播放时间(浮点型的s) audio_clock - audio_buffer_before_play
  double get_audio_clock();
  double get_video_clock();
  double get_external_clock();
  // 音视频同步代码
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

public:
  FFPlayer();
  ~FFPlayer();
  // got audio frame api
  void getAudioFrame(unsigned char *stream, int len);
  // event api
  void set_event(FFEvent* e) { event_ = e; }
  FFEvent* event(FFEvent* e) { return event_; }

  bool muted() const { return muted_; }
  void set_muted(bool mute);
  bool paused() const { return paused_; }
  void set_paused(bool v);
  void pause() { set_paused(true); }
  void resume() { set_paused(false); }

  void seek(double pos);
  double position() { return get_master_clock(); }
  double duration();

  bool Open(const char* path);
  bool isOpen() const { return !quit; }
  bool Close();
};

