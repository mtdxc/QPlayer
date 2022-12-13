
 不知大家有没有发现FFmpeg长时间解码会出现延时增大（特别是在丢包的情况下）？如果在播放本地文件，这个问题是没有影响的。但是如果播放的是实时流，则图像的延时就越来越大。本人是做安防监控的，很多招标项目对解码器的图像延时都有要求：不能高于250毫秒。所以，对实时性要求高的场合，要尽量降低图像的延时。

网络摄像头从采集图像到通过网络传输到客户端解码的过程中都会产生延时，一般延时有几部分组成：编码延时、网络传输延时、解码延时。
- 编码延时是由编码器产生的，只能由设备方去改善，如果是FFmpeg软编码的方式，也是需要优化的，优化方法见我的另外一篇博文怎么降低FFmpeg的编码延时。
- 网络传输延时一般很小。
- 解码延时是产生延时的一个很重要的部分，所以我们要想办法降低解码时的延时。

## 优化解码延时的方法我归纳如下：


第一，减少视频的缓冲帧数，尽量收到视频就送去解码。

第二，从网络接收数据，解码，还有显示操作用多线程处理，从而提高并发性。一般地，我们做播放器会把数据接收+解码放在一个线程处理，而显示放在另外一个线程；或者数据接收用一个线程，而解码+显示放在另外一个线程。中间实现一个缓冲队列，把第一个线程输出的数据扔到缓冲区，而第二个线程从队列里取数据，读到一帧就马上解码或显示。这样可以保证数据包可以得到最快的速度的处理，不会因为网络阻塞而影响后面的处理流程。

第三，清空FFmpeg编码器的缓冲区。当前面两种方法都无效时，这种方法可能就最管用了。在网络环境中，传输数据用UDP经常有丢包，而丢包很容易造成FFmpeg解码器缓冲的帧数增加。解决办法是：隔一段时间清空解码器缓存。伪代码如下：

```cpp
int got_picture = 0; //重置解码器，在解码I帧之前清空解码器缓存 
//m_bReset为True，表示即将清空解码器缓存
if(m_bReset && nFrameType ==1) {
   avcodec_flush_buffers(m_pVideoCodecCtx);
   m_bReset = FALSE;
}
AVPacket avpkt;
av_init_packet(&avpkt); 
avpkt.size = inLen; avpkt.data = inbuf; 
int len = avcodec_decode_video2(m_pVideoCodecCtx, picture, &got_picture, &avpkt);
if (len < 0) {
  TRACE("Error while decoding frame Len %d\n", inLen);  
  return FALSE;
}
```
不过要注意的是：不能任何一帧前都可以进行清空操作，理想是在I帧之前做清空，否则图像会有不连续或马赛克的现象。

## 设置低延迟播放

无论是ffmpeg.exe还是libffmpeg等，均有可有效优化延迟的参数，现在列出部分实际工作中使用的记录。


```cpp
AVDictionary *options = NULL;
av_dict_set(&options, "fflags", "nobuffer", 0); //无缓存，解码时有效
//av_dict_set(&options, "timeout", "10", 0);
if (avformat_open_input(&m_pFormatContext, filename, NULL, &options) != 0) {
  log_print(LOG_ERR, "avformat_open_input failed, %s\r\n", filename);
  return FALSE;
}
//减低延迟操作：减少探测的时间
m_pFormatContext->probesize = 100 * 1024;
m_pFormatContext->max_analyze_duration = 5*AV_TIME_BASE;

avformat_find_stream_info(m_pFormatContext, NULL);
```