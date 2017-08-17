extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/avstring.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}
#include <windows.h>
#include "FFPlayer.h"
void Output(const char* fmt, ...) {
  char sztmp[1024] = { 0 };
  va_list vl;
  va_start(vl, fmt);
  int n = vsprintf(sztmp, fmt, vl);
  if (sztmp[n - 1] != '\n') {
    sztmp[n++] = '\n';
    sztmp[n] = 0;
  }
  fprintf(stderr, sztmp);
  OutputDebugStringA(sztmp);
  //vfprintf(stderr, fmt, vl);
  va_end(vl);
}

static AVPacket flush_pkt; // packet for seek

static int decode_interrupt_cb(void *opaque) {
  return !((FFPlayer*)opaque)->isOpen();
}

void VideoPicture::Clear()
{
  if (bmp) free(bmp);
  bmp = NULL;
  width = height = 0;
}

uint8_t* VideoPicture::GetBuffer(int w, int h)
{
  if (width != w || height != h) {
    Clear();
    bmp = (uint8_t*)malloc(w * h * 3);
    if (bmp) {
      width = w;
      height = h;
    }
  }
  return bmp;
}

int PacketQueue::put(AVPacket *pkt)
{
  std::unique_lock<std::mutex> l(mutex);
  if (pkt) {
    size += pkt->size;
    list.push_back(pkt);
    // notify add
    cond.notify_one();
  }
  return 0;
}

int PacketQueue::get(AVPacket* &pkt, int block)
{
  int ret;
  std::unique_lock<std::mutex> l(mutex);
  for (;;) {
    /*
    if (global_video_state->quit) {
      ret = -1;
      break;
    }
    */
    if (list.size()) {
      pkt = list.front();
      size -= pkt->size;
      list.pop_front();
      ret = 1;
      break;
    }
    else if (!block) {
      ret = 0;
      break;
    }
    else {
      cond.wait(l);
    }
  }
  return ret;
}

FFPlayer::FFPlayer()
{
  av_register_all();
  avformat_network_init();
  av_init_packet(&flush_pkt);
  flush_pkt.data = (unsigned char *)"FLUSH";

  sws_video = NULL;
  swr_audio = NULL;
  pFormatCtx = NULL;
  io_context = NULL;
  audio_st = video_st = NULL;
  event_ = NULL;
  audio_pkt = NULL;
  pictq_size = pictq_rindex = pictq_windex = 0;
}

FFPlayer::~FFPlayer()
{
  Close();
}

bool FFPlayer::Open(const char* path)
{
  av_strlcpy(filename, path, 1024);

  quit = muted_ = paused_ = false;
  av_sync_type = DEFAULT_AV_SYNC_TYPE;
  //schedule_refresh(is, 40);
  parse_tid = std::thread(&FFPlayer::demuxer_thread_func, this);
  return true;
}

bool FFPlayer::Close()
{
  quit = true;
  if (parse_tid.joinable())
    parse_tid.join();
  if (render_tid.joinable())
    render_tid.join();
  if (video_tid.joinable())
    video_tid.join();
  swr_free(&swr_audio);
  if (sws_video) {
    sws_freeContext(sws_video);
    sws_video = NULL;
  }
  // free data
  if (audio_st && audio_st->codec) {
    avcodec_close(audio_st->codec);
    audio_st = NULL;
  }
  if (video_st && video_st->codec) {
    avcodec_close(video_st->codec);
    video_st = NULL;
  }
  if (pFormatCtx) {
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);
    pFormatCtx = NULL;
  }
  if (io_context) {
    avio_close(io_context);
    io_context = NULL;
  }
  audioq.clear();
  videoq.clear();
  return true;
}

double FFPlayer::get_audio_clock()
{
  double pts;
  int hw_buf_size, bytes_per_sec, n;

  pts = audio_clock; /* maintained in the audio thread */
  hw_buf_size = audio_buf_size - audio_buf_index;
  bytes_per_sec = 0;
  n = audio_st->codec->channels * 2;
  if (audio_st) {
    bytes_per_sec = audio_st->codec->sample_rate * n;
  }
  if (bytes_per_sec) {
    pts -= (double)hw_buf_size / bytes_per_sec;
  }
  return pts;
}

double FFPlayer::get_video_clock()
{
  double delta = (av_gettime() - video_current_pts_time) / 1000000.0;
  return video_current_pts + delta;
}

double FFPlayer::get_external_clock()
{
  return av_gettime() / 1000000.0;
}

int FFPlayer::synchronize_audio(short *samples, int samples_size, double pts)
{
  int n;
  double ref_clock;

  n = 2 * audio_st->codec->channels;

  if (av_sync_type != AV_SYNC_AUDIO_MASTER) {
    double diff, avg_diff;
    int wanted_size, min_size, max_size /*, nb_samples */;

    ref_clock = get_master_clock();
    diff = get_audio_clock() - ref_clock;

    if (diff < AV_NOSYNC_THRESHOLD) {
      // accumulate the diffs
      audio_diff_cum = diff + audio_diff_avg_coef * audio_diff_cum;
      if (audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
        audio_diff_avg_count++;
      }
      else {
        avg_diff = audio_diff_cum * (1.0 - audio_diff_avg_coef);
        if (fabs(avg_diff) >= audio_diff_threshold) {
          wanted_size = samples_size + ((int)(diff * audio_st->codec->sample_rate) * n);
          min_size = samples_size * ((100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100);
          max_size = samples_size * ((100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100);
          if (wanted_size < min_size) {
            wanted_size = min_size;
          }
          else if (wanted_size > max_size) {
            wanted_size = max_size;
          }
          if (wanted_size < samples_size) {
            /* remove samples */
            samples_size = wanted_size;
          }
          else if (wanted_size > samples_size) {
            uint8_t *samples_end, *q;
            int nb;

            /* add samples by copying final sample*/
            nb = (samples_size - wanted_size);
            samples_end = (uint8_t *)samples + samples_size - n;
            q = samples_end + n;
            while (nb > 0) {
              memcpy(q, samples_end, n);
              q += n;
              nb -= n;
            }
            samples_size = wanted_size;
          }
        }
      }
    }
    else {
      /* difference is TOO big; reset diff stuff */
      audio_diff_avg_count = 0;
      audio_diff_cum = 0;
    }
  }
  return samples_size;
}

int FFPlayer::decode_frame_from_packet(AVFrame& decoded_frame)
{
  int64_t src_ch_layout, dst_ch_layout;
  int src_rate, dst_rate;
  uint8_t **src_data = NULL, **dst_data = NULL;
  int src_nb_channels = 0, dst_nb_channels = 0;
  int src_linesize, dst_linesize;
  int src_nb_samples, dst_nb_samples, max_dst_nb_samples;
  enum AVSampleFormat src_sample_fmt, dst_sample_fmt;
  int dst_bufsize;
  int ret;

  src_nb_samples = decoded_frame.nb_samples;
  src_linesize = (int)decoded_frame.linesize;
  src_data = decoded_frame.data;

  if (decoded_frame.channel_layout == 0) {
    decoded_frame.channel_layout = av_get_default_channel_layout(decoded_frame.channels);
  }

  src_rate = decoded_frame.sample_rate;
  dst_rate = decoded_frame.sample_rate;
  src_ch_layout = decoded_frame.channel_layout;
  dst_ch_layout = decoded_frame.channel_layout;
  src_sample_fmt = (AVSampleFormat)decoded_frame.format;
  dst_sample_fmt = AV_SAMPLE_FMT_S16;
  if (!swr_audio)
    swr_audio = swr_alloc();

  if (!swr_audio){
    Output("Could not allocate resampler context\n");
    return -1;
  }
  av_opt_set_int(swr_audio, "in_channel_layout", src_ch_layout, 0);
  av_opt_set_int(swr_audio, "out_channel_layout", dst_ch_layout, 0);
  av_opt_set_int(swr_audio, "in_sample_rate", src_rate, 0);
  av_opt_set_int(swr_audio, "out_sample_rate", dst_rate, 0);
  av_opt_set_sample_fmt(swr_audio, "in_sample_fmt", src_sample_fmt, 0);
  av_opt_set_sample_fmt(swr_audio, "out_sample_fmt", dst_sample_fmt, 0);

  /* initialize the resampling context */
  if ((ret = swr_init(swr_audio)) < 0) {
    Output("Failed to initialize the resampling context\n");
    return -1;
  }

  /* allocate source and destination samples buffers */
  src_nb_channels = av_get_channel_layout_nb_channels(src_ch_layout);
  ret = av_samples_alloc_array_and_samples(&src_data, &src_linesize, src_nb_channels, src_nb_samples, src_sample_fmt, 0);
  if (ret < 0) {
    Output("Could not allocate source samples\n");
    return -1;
  }

  /* compute the number of converted samples: buffering is avoided
  * ensuring that the output buffer will contain at least all the
  * converted input samples */
  max_dst_nb_samples = dst_nb_samples = av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);

  /* buffer is going to be directly written to a rawaudio file, no alignment */
  dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
  ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels, dst_nb_samples, dst_sample_fmt, 0);
  if (ret < 0) {
    Output("Could not allocate destination samples\n");
    return -1;
  }

  /* compute destination number of samples */
  dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_audio, src_rate) + src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);

  /* convert to destination format */
  ret = swr_convert(swr_audio, dst_data, dst_nb_samples, (const uint8_t **)decoded_frame.data, src_nb_samples);
  if (ret < 0) {
    Output("Error while converting\n");
    return -1;
  }

  dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels, ret, dst_sample_fmt, 1);
  if (dst_bufsize < 0) {
    Output("Could not get sample buffer size\n");
    return -1;
  }

  memcpy(audio_buf, dst_data[0], dst_bufsize);

  if (src_data) {
    av_freep(&src_data[0]);
  }
  av_freep(&src_data);

  if (dst_data) {
    av_freep(&dst_data[0]);
  }
  av_freep(&dst_data);
  return dst_bufsize;
}

int FFPlayer::audio_decode_frame(double *pts_ptr)
{
  int len1, data_size = 0, n;
  double pts;
  AVFrame audio_frame = {0};
  for (;;) {
    while (audio_pkt_size > 0) {
      int got_frame = 0;
      AVPacket pkt = *audio_pkt;
      pkt.data = audio_pkt_data;
      pkt.size = audio_pkt_size;
      // @todo make new/move packet data?
      len1 = avcodec_decode_audio4(audio_st->codec, &audio_frame, &got_frame, &pkt);
      if (len1 < 0) {
        /* if error, skip frame */
        audio_pkt_size = 0;
        break;
      }
      if (got_frame)
      {
        if (audio_frame.format != AV_SAMPLE_FMT_S16) {
          data_size = decode_frame_from_packet(audio_frame);
        }
        else {
          data_size = av_samples_get_buffer_size(
              NULL,
              audio_st->codec->channels,
              audio_frame.nb_samples,
              audio_st->codec->sample_fmt,
              1);
          memcpy(audio_buf, audio_frame.data[0], data_size);
        }
        av_frame_unref(&audio_frame);
      }
      audio_pkt_data += len1;
      audio_pkt_size -= len1;
      if (data_size <= 0) {
        /* No data yet, get more frames */
        continue;
      }
      pts = audio_clock;
      *pts_ptr = pts;
      n = 2 * audio_st->codec->channels;
      audio_clock += (double)data_size / (double)(n * audio_st->codec->sample_rate);

      /* We have data, return it and come back for more later */
      return data_size;
    }
    if (audio_pkt) {
      av_packet_free(&audio_pkt);
    }

    if (quit) {
      return -1;
    }
    /* next packet */
    if (audioq.get(audio_pkt, 1) < 0) {
      return -1;
    }
    if (audio_pkt == &flush_pkt) {
      avcodec_flush_buffers(audio_st->codec);
      audio_pkt = NULL;
      continue;
    }
    audio_pkt_data = audio_pkt->data;
    audio_pkt_size = audio_pkt->size;
    /* if update, update the audio clock w/pts */
    if (audio_pkt->pts != AV_NOPTS_VALUE) {
      audio_clock = av_q2d(audio_st->time_base)*audio_pkt->pts;
    }
  }
}

int64_t FFPlayer::position()
{
  double clock = get_master_clock();
  return clock * 1000;
}

int64_t FFPlayer::duration()
{
  if (pFormatCtx)
    return pFormatCtx->duration / 1000;// AV_TIME_BASE;
  return 0;
}

void FFPlayer::getAudioFrame(unsigned char *stream, int len)
{
  int len1, audio_size;
  double pts;

  while (len > 0) {
    if (audio_buf_index >= audio_buf_size) {
      /* We have already sent all our data; get more */
      audio_size = audio_decode_frame(&pts);
      if (audio_size < 0) {
        /* If error, output silence */
        audio_buf_size = SDL_AUDIO_BUFFER_SIZE;
        memset(audio_buf, 0, audio_buf_size);
      }
      else {
        audio_size = synchronize_audio((int16_t *)audio_buf, audio_size, pts);
        audio_buf_size = audio_size;
      }
      audio_buf_index = 0;
    }

    // copy buffer
    len1 = audio_buf_size - audio_buf_index;
    if (len1 > len)
      len1 = len;
    if (muted_) {
      memset(stream, 0, len1);
    }
    else {
      memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
    }
    len -= len1;
    stream += len1;
    audio_buf_index += len1;
  }
}

int FFPlayer::queue_picture(AVFrame *pFrame, double pts)
{
  //int dst_pix_fmt;
  AVPicture pict;

  /* wait until we have space for a new pic */
  std::unique_lock<std::mutex> l(pictq_mutex);
  while (pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !quit) {
    pictq_cond.wait(l);
  }
  l.unlock();

  if (quit)
    return -1;

  // windex is set to 0 initially
  VideoPicture* vp = &pictq[pictq_windex];
  if (vp->width != video_st->codec->width || vp->height != video_st->codec->height) {
    if (sws_video) {
      sws_freeContext(sws_video);
      sws_video = NULL;
    }
  }
  uint8_t* p = vp->GetBuffer(video_st->codec->width, video_st->codec->height);
  if (p) {
    // Convert the image into YUV format that SDL uses
    if(!sws_video)
      sws_video = sws_getContext(
          video_st->codec->width,
          video_st->codec->height,
          video_st->codec->pix_fmt,
          video_st->codec->width,
          video_st->codec->height,
          AV_PIX_FMT_RGB24,
          SWS_BILINEAR,
          NULL,
          NULL,
          NULL
        );

    AVPicture pict = { 0 };
    pict.data[0] = p;
    pict.linesize[0] = vp->width * 3;
    // 解决Windows翻转问题
    int height = video_st->codec->height;
    /*
    pFrame->data[0] += pFrame->linesize[0] * (height - 1);
    pFrame->linesize[0] *= -1;
    pFrame->data[1] += pFrame->linesize[1] * (height / 2 - 1);
    pFrame->linesize[1] *= -1;
    pFrame->data[2] += pFrame->linesize[2] * (height / 2 - 1);
    pFrame->linesize[2] *= -1;
    */
    std::swap(pFrame->linesize[1], pFrame->linesize[2]);
    std::swap(pFrame->data[1], pFrame->data[2]);
    sws_scale( sws_video,
      pFrame->data,
      pFrame->linesize,
      0,
      height,
      pict.data,
      pict.linesize
    );
    vp->pts = pts;
    //sws_freeContext(sws_video);
    l.lock();
    /* now we inform our display thread that we have a pic ready */
    if (++pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) {
      pictq_windex = 0;
    }
    pictq_size++;
  }
  return 0;
}

double FFPlayer::synchronize_video(AVFrame *src_frame, double pts)
{
  double frame_delay;

  if (pts != 0) {
    /* if we have pts, set video clock to it */
    video_clock = pts;
  }
  else {
    /* if we aren't given a pts, set it to the clock */
    pts = video_clock;
  }
  /* update the video clock */
  frame_delay = av_q2d(video_st->codec->time_base);
  /* if we are repeating a frame, adjust clock accordingly */
  frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
  video_clock += frame_delay;
  return pts;
}

int FFPlayer::video_decode_func()
{
  AVPacket *packet = NULL;
  int frameFinished;
  double pts;

  AVFrame* pFrame = av_frame_alloc();
  for (;;) {
    if (videoq.get(packet, 1) < 0) {
      // means we quit getting packets
      break;
    }
    if (packet == &flush_pkt) {
      avcodec_flush_buffers(video_st->codec);
      continue;
    }
    if (packet == NULL)
      continue;
    pts = 0;

    // Save global pts to be stored in pFrame in first call
    // global_video_pkt_pts = packet->pts;
    // Decode video frame
    avcodec_decode_video2(video_st->codec, pFrame, &frameFinished, packet);
    if (packet->dts != AV_NOPTS_VALUE) {
      pts = packet->dts;
    }
    else if (pFrame->opaque && *(uint64_t*)pFrame->opaque != AV_NOPTS_VALUE) {
      pts = *(uint64_t *)pFrame->opaque;
    }
    else {
      pts = 0;
    }
    pts *= av_q2d(video_st->time_base);

    // Did we get a video frame?
    if (frameFinished) {
      pts = synchronize_video(pFrame, pts);
      if (queue_picture(pFrame, pts) < 0) {
        break;
      }
      av_frame_unref(pFrame);
    }
    av_packet_free(&packet);
  }
  av_frame_free(&pFrame);
  return 0;
}

void FFPlayer::video_render_func() {
  VideoPicture *vp;
  double actual_delay, delay, sync_threshold, ref_clock, diff;
  while (!quit) {
    if (!video_st || pictq_size == 0) {
      Sleep(10);
      continue;
    }
    vp = &pictq[pictq_rindex];

    video_current_pts = vp->pts;
    video_current_pts_time = av_gettime();

    delay = vp->pts - frame_last_pts; /* the pts from last time */
    if (delay <= 0 || delay >= 1.0) {
      /* if incorrect delay, use previous one */
      delay = frame_last_delay;
    }
    /* save for next time */
    frame_last_delay = delay;
    frame_last_pts = vp->pts;

    /* update delay to sync to audio if not master source */
    if (av_sync_type != AV_SYNC_VIDEO_MASTER) {
      ref_clock = get_master_clock();
      diff = vp->pts - ref_clock;

      /* Skip or repeat the frame. Take delay into account
      FFPlay still doesn't "know if this is the best guess." */
      sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;
      if (fabs(diff) < AV_NOSYNC_THRESHOLD) {
        if (diff <= -sync_threshold) {
          delay = 0;
        }
        else if (diff >= sync_threshold) {
          delay = 2 * delay;
        }
      }
    }

    frame_timer += delay;
    /* computer the REAL delay */
    actual_delay = frame_timer - (av_gettime() / 1000000.0);
    if (actual_delay < 0.010) {
      /* Really it should skip the picture instead */
      actual_delay = 0.010;
    }
    Sleep((int)(actual_delay * 1000 + 0.5));

    /* show the picture! */
    display_video(vp);

    /* update queue for next picture! */
    if (++pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
      pictq_rindex = 0;
    }
    std::unique_lock<std::mutex> l(pictq_mutex);
    pictq_size--;
    pictq_cond.notify_all();
  }
}

void FFPlayer::display_video(VideoPicture * vp)
{
  if (event_)
    event_->onVideoFrame(vp);
}

int FFPlayer::stream_component_open(int stream_index)
{
  AVCodecContext *codecCtx = NULL;
  AVCodec *codec = NULL;
  AVDictionary *optionsDict = NULL;

  if (stream_index < 0 || stream_index >= pFormatCtx->nb_streams) {
    return -1;
  }

  // Get a pointer to the codec context for the video stream
  codecCtx = pFormatCtx->streams[stream_index]->codec;

  codec = avcodec_find_decoder(codecCtx->codec_id);
  /* Init the decoders, with or without reference counting */
  // av_dict_set(&optionsDict, "refcounted_frames", refcount ? "1" : "0", 0);
  if (!codec || (avcodec_open2(codecCtx, codec, &optionsDict) < 0)) {
    Output("Unsupported codec!\n");
    return -1;
  }

  switch (codecCtx->codec_type) {
  case AVMEDIA_TYPE_AUDIO:
    audio_buf_size = 0;
    audio_buf_index = 0;

    /* averaging filter for audio sync */
    audio_diff_avg_coef = exp(log(0.01 / AUDIO_DIFF_AVG_NB));
    audio_diff_avg_count = 0;
    /* Correct audio only if larger error than this */
    audio_diff_threshold = 2.0 * SDL_AUDIO_BUFFER_SIZE / codecCtx->sample_rate;

    if (event_ &&
      event_->onAudioStream(stream_index, codec->id, codecCtx->sample_rate, codecCtx->channels)) {
      audio_st = pFormatCtx->streams[stream_index];
      audioStream = stream_index;
      audioq.clear();
      av_sync_type = AV_SYNC_AUDIO_MASTER;
    }
    break;
  case AVMEDIA_TYPE_VIDEO:
    frame_timer = (double)av_gettime() / 1000000.0;
    frame_last_delay = 40e-3;
    video_current_pts_time = av_gettime();

    videoq.clear();
    if (event_ &&
      event_->onVideoStream(stream_index, codec->id, codecCtx->width, codecCtx->height)) {
      video_st = pFormatCtx->streams[stream_index];
      videoStream = stream_index;
      video_tid = std::thread(&FFPlayer::video_decode_func, this);
      render_tid = std::thread(&FFPlayer::video_render_func, this);
      av_sync_type = AV_SYNC_VIDEO_MASTER;
    }
    break;
  default:
    break;
  }

  return 0;
}

void FFPlayer::seek(double pos, int rel)
{
  if (!seek_req) {
    seek_pos = (int64_t)(pos * AV_TIME_BASE);
    seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
    seek_req = 1;
  }
}

int FFPlayer::demuxer_thread_func()
{
  AVFormatContext *pFormatCtx = NULL;

  AVDictionary *io_dict = NULL;
  AVIOInterruptCB callback;

  int video_index = -1;
  int audio_index = -1;
  int i;

  videoStream = -1;
  audioStream = -1;

  // will interrupt blocking functions if we quit!
  callback.callback = decode_interrupt_cb;
  callback.opaque = this;
  if (avio_open2(&io_context, filename, 0, &callback, &io_dict))
  {
    Output("Unable to open I/O for %s\n", filename);
    return -1;
  }

  // Open video file
  if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
    return -1; // Couldn't open file

  this->pFormatCtx = pFormatCtx;

  // Retrieve stream information
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    return -1; // Couldn't find stream information

               // Dump information about file onto standard error
  av_dump_format(pFormatCtx, 0, filename, 0);

  // Find the first video stream
  for (i = 0; i < pFormatCtx->nb_streams; i++) {
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
      video_index < 0) {
      video_index = i;
    }
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
      audio_index < 0) {
      audio_index = i;
    }
  }
  if (video_index >= 0) {
    stream_component_open(video_index);
  }
  if (audio_index >= 0) {
    stream_component_open(audio_index);
  }

  if (videoStream < 0 && audioStream < 0) {
    Output("%s: could not open codecs\n", filename);
    goto fail;
  }

  AVPacket packet;
  // main decode loop
  while (!quit) {
    // seek stuff goes here
    if (seek_req) {
      int stream_index = -1;
      int64_t seek_target = seek_pos;

      if (videoStream >= 0)
        stream_index = videoStream;
      else if (audioStream >= 0)
        stream_index = audioStream;

      if (stream_index >= 0) {
        seek_target = av_rescale_q(seek_target, { 1, AV_TIME_BASE }, pFormatCtx->streams[stream_index]->time_base);
      }
      if (av_seek_frame(pFormatCtx, stream_index, seek_target, seek_flags) < 0) {
        Output("%s: error while seeking\n", pFormatCtx->filename);
      }
      else {
        if (audioStream >= 0) {
          audioq.clear();
          audioq.put(&flush_pkt);
        }
        if (videoStream >= 0) {
          videoq.clear();
          videoq.put(&flush_pkt);
        }
      }
      seek_req = 0;
    }
    if (paused_) {
      ::Sleep(10);
      continue;
    }
    // delay for packet queue full
    if (audioq.size > MAX_AUDIOQ_SIZE || videoq.size > MAX_VIDEOQ_SIZE) {
      ::Sleep(10);
      continue;
    }
    // read packet
    if (av_read_frame(pFormatCtx, &packet) < 0) {
      if (pFormatCtx->pb->error == 0) {
        ::Sleep(100); /* no error; wait for user input */
        continue;
      }
      else {
        break;
      }
    }
    // Is this a packet from the video stream?
    if (packet.stream_index == videoStream) {
      videoq.put(av_packet_clone(&packet));
    }
    else if (packet.stream_index == audioStream) {
      audioq.put(av_packet_clone(&packet));
    }
    av_packet_unref(&packet);
  }
  /* all done - wait for it */
  while (!quit) {
    Sleep(100);
  }
fail:
  {
    quit = true;
  }
  return 0;
}


