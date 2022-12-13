## FFmpeg超低延迟视频推流笔记

云游戏平台推流和普通直播平台最大不同有两点，一个是码率，我们希望用户能尽可能用低的网络体验好的效果，毕竟玩的人是自己，还有一点就是延迟了，普通直播延迟5秒左右都属于正常，通常降低到1秒左右就属于超低延迟了，而我们的要求是降低到50毫秒以下，中间遇到了不少坑，记录一下。  
音频延迟可以参考我的另一篇：

## [FFmpeg RTP 100ms以下超低音频流推送设置](https://blog.csdn.net/weixin_44259356/article/details/102698197)

[https://blog.csdn.net/weixin\_44259356/article/details/102698197](https://blog.csdn.net/weixin_44259356/article/details/102698197)  
原文：[https://blog.csdn.net/weixin\_44259356/article/details/103287477](https://blog.csdn.net/weixin_44259356/article/details/103287477)

# libx264软编设置

首先可以通过指令查看编码支持的参数配置，libx264官网也有文档，写的比较详细也可以看官网，指令如下：

```bash
ffmpeg -h encoder=libx264
```

正常软编推流主要有两个问题，第一当画面不动时播放端流容易中断，第二延迟高。1080p，30帧，关键帧间隔30时，设置如下：

```bash
tune=zerolatency sc_threshold=499 profile=high preset=ultrafast
```

这是obs视频额外编码设置格式，正常使用ffmpeg推流则是如下格式：

```bash
-tune zerolatency -sc_threshold 499 -profile high -preset ultrafast
```

## tune

设置编码方式为零延迟，还有其他的编码方式，比如电影，动画等，具体可以通过上面命令查看，这里不多赘述除了解决问题以外的参数，这个参数引入后能极大的降低延迟，但是画面容易不稳定，容易播放中断。

## sc\_threshold

设置场景更改检测的阈值，主要是作用与当画面运动时额外的数据记录。值越高画面会越流畅，但是相同码率下画面会变模糊，值过低运动时画面会出现卡顿，499为测试调整的值，不同的其他配置可以测试调整。

## profile

设置编码画质基本为high，主要有三个档次Baseline，Main，High。级别越高，画质越好，编码开销越大，另外高级别的编码有些低级设备可能会出现无法支持解码播放的问题。这里为了画面效果选择high。

## preset

调整编码预设等级，主要有：ultrafast，superfast，veryfast，faster，fast，medium，slow，slower，veryslow，placebo。  
和profile一样等级越高画面质量越好，但是编码越慢，这里设置为最低，如果不设置为最低，在零延迟编码下windows OBS会出现当画面为全黑，推流视频无法播放的问题，linux obs貌似没有这个问题，这里不涉及linux所有配置全是windows下。

## 效果：

1080p 30帧 6000码率下延迟能降低到12毫秒左右。  
![在这里插入图片描述](https://www.codeleading.com/imgrdrct/https://img-blog.csdnimg.cn/20191128101643308.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl80NDI1OTM1Ng==,size_16,color_FFFFFF,t_70)

# h264\_nvenc硬编设置

硬编和软编相比，在更低的码率下能有更好的推流效果，但是延迟会更高。设置如下：

```bash
preset=llhq profile=high level=4 rc=vbr_hq zerolatency=1 coder=auto
```

ffmpeg命令行设置参考上面软编。

## preset

llhq为硬编特有，低延迟高质量编码

## level

设置编码约束等级，1080p 30帧用4.0刚刚好，再往上就得提高等级最高为5.1，支持4k30帧，

## rc

设置编码码率方式，vbr\_hq为可变，高质量编码。另外还有cbr恒定码率等方式

## zerolatency

设置启用零延迟编码，默认为false，这里设置为1或者true都行，tune为软编特有参数。zerolatency为硬编设置，实测能降低几毫秒延迟左右不少特别大。

## coder

设置编码类型，这里自动选择就好。

## 效果：

![在这里插入图片描述](https://www.codeleading.com/imgrdrct/https://img-blog.csdnimg.cn/20191128104557245.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl80NDI1OTM1Ng==,size_16,color_FFFFFF,t_70)  
延迟29毫秒，比软编延迟差不多大了三倍，但是同样的效果需要得码率也只有软编的三分之一。另外我用的gpu是1660Ti，nvenc官网说用2060以上gpu效果会更好，目前显卡还没到，没测试过，

## 最后

还有个nvenc_h264编码，不过我看命令行提示以及被弃用了，此外还测试过vp8，vp9，h265等编码，vp8效果不好，vp9编码太慢，h265，webrtc目前不支持，等原因这里不采用。



# ffmpeg RTSP推流命令
-c copy 的方式 CPU 占用低，但要求 RTSP 视频源为 H264，否则大部分浏览器不兼容。
如果不用 -c copy 则会进行转码，此时必须使用子码流输入，否则CPU占用率高。

如果一定要使用 -c copy，则可以将子码流配置成 H264，作为输入源。


# H5最佳方案：播放速度快、H5可以播放、CPU占用低，用子码流（H265/H264都可以）
# 如果 -hls_wrap 无法使用，可换成 -hls_flags
ffmpeg -rtsp_transport tcp -re -i "rtsp://admin:123456@192.168.123.22:554/Streaming/Channels/102" -f hls -crf 23 -tag:v hvc1 -preset ultrafast -maxrate 1M -bufsize 300k -r 10 -g 15 -movflags +faststart -tune zerolatency -hls_time 1 -hls_list_size 5 -hls_wrap 6 -start_number 1 -hls_allow_cache 0 -threads 1 -loglevel warning -y -an  "C:\demo-service\demo-nginx\html\hls\123.22.m3u8"


# 播放速度最快、CPU占用最低，但如视频源不是265则H5无法播放
# 如果 -hls_wrap 无法使用，可换成 -hls_flags
ffmpeg -rtsp_transport tcp -re -i "rtsp://admin:123456@192.168.123.22:554/Streaming/Channels/101" -f hls -c:v copy -preset ultrafast -tune zerolatency -hls_list_size 5 -hls_wrap 6 -r 10 -an  "C:\demo-service\demo-nginx\html\hls\123.27.m3u8"

# 播放速度快、H5可以播放，但CPU占用高
ffmpeg -rtsp_transport tcp -re -i "rtsp://admin:123456@192.168.123.22:554/Streaming/Channels/101" -f hls -crf 23 -preset ultrafast -maxrate 1M -bufsize 300k -r 10 -g 20 -movflags +faststart -tune zerolatency -hls_time 1 -hls_list_size 5 -hls_wrap 6 -start_number 1 -hls_allow_cache 0 -threads 1 -loglevel warning -y "C:\demo-service\demo-nginx\html\hls\123.22.m3u8"

# windows 批处理脚本： %% 转义 %
set password=123456,.%%2F
start /B ffmpeg -rtsp_transport tcp -re -i "rtsp://admin:%password%@192.168.123.22:554/Streaming/Channels/102" -f hls -crf 23 -tag:v hvc1 -preset ultrafast -maxrate 1M -bufsize 300k -r 10 -g 15 -movflags +faststart -tune zerolatency -hls_time 1 -hls_list_size 5 -hls_wrap 6 -start_number 1 -hls_allow_cache 0 -threads 1 -loglevel warning -y -an  "C:\demo-service\demo-nginx\html\hls\123.22.m3u8"

# 重连机制
ffmpeg -rtsp_transport tcp -re -i "rtsp://admin:123456@192.168.123.22:554/Streaming/Channels/102" -reconnect 1 -reconnect_at_eof 1 -reconnect_streamed 1 -reconnect_on_network_error 1 -reconnect_on_http_error 1 -reconnect_delay_max 4096 -f hls -crf 23 -tag:v hvc1 -preset ultrafast -maxrate 1M -bufsize 300k -r 10 -g 15 -movflags +faststart -tune zerolatency -hls_time 1 -hls_list_size 5 -hls_wrap 6 -start_number 1 -hls_allow_cache 0 -threads 1 -loglevel warning -y -an  "C:\demo-service\demo-nginx\html\hls\123.22.m3u8"
