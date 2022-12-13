## [FFmpeg RTP超低音频流推送设置](https://blog.csdn.net/weixin_44259356/article/details/102698197)

由于使用obs推流音频始终有各种各样的问题，所以目前打算直接使用[FFmpeg](https://so.csdn.net/so/search?q=FFmpeg&spm=1001.2101.3001.7020)推送音频流，期间遇到了各种坑，特此记录。  
原文链接：[https://blog.csdn.net/weixin\_44259356/article/details/102698197](https://blog.csdn.net/weixin_44259356/article/details/102698197)

## 视频+音频

首先安装好FFmpeg，然后可以用以下命令测试，我们转发服务器用的是janus，也可以用其他vlc等。

```bash
ffmpeg  -re -i C:\Users\rong\Videos\xxx.mp4 -an -vcodec copy -vcodec hevc -f rtp rtp://10.33.250.241:8004 -vn -acodec copy -acodec opus -strict -2 -f rtp rtp://10.33.250.241:8002
```

## 音频推流

```bash
ffmpeg  -re -i C:\Users\rong\Videos\xxx.mp4 -vn -acodec copy -f rtp rtp://10.33.250.241:8002
ffmpeg  -re -i C:\Users\rong\Videos\1.opus -vn -acodec copy -f rtp rtp://10.33.250.241:8002
```

## 获取输入设备列表

```bash
ffmpeg -devices
ffmpeg -f dshow -list_devices true -i ""
```

## 采集麦克风并录制

这里先查看输入设备名称，中文可能会乱码，就用id代替如下，"@device\_cm\_{33D9A762-90C8-11D0-BD43-00A0C911CE86}\\wave\_{56E9ADCA-F789-4439-BE53-5C6DCC324AAA}"，我的设备id

```bash
ffmpeg -f dshow -i audio="@device_cm_{33D9A762-90C8-11D0-BD43-00A0C911CE86}\wave_{56E9ADCA-F789-4439-BE53-5C6DCC324AAA}"  C:\Users\rong\Videos\1.opus
```

## 采集麦克风编码并录制

```bash
ffmpeg -f dshow  -i audio="@device_cm_{33D9A762-90C8-11D0-BD43-00A0C911CE86}\wave_{56E9ADCA-F789-4439-BE53-5C6DCC324AAA}" -acodec opus -strict experimental  -preset:v ultrafast  C:\Users\rong\Videos\1.opus
```

## 采集麦克风并rtp推流

```bash
ffmpeg -f dshow -i audio="@device_cm_{33D9A762-90C8-11D0-BD43-00A0C911CE86}\wave_{56E9ADCA-F789-4439-BE53-5C6DCC324AAA}" -vn -acodec copy -f rtp rtp://10.33.250.241:8002
```

## 麦克风推流音频opus编码

因为云游戏要求超低延迟所以采用opus编码

```bash
ffmpeg -f dshow -i audio="@device_cm_{33D9A762-90C8-11D0-BD43-00A0C911CE86}\wave_{56E9ADCA-F789-4439-BE53-5C6DCC324AAA}" -vn -acodec copy -acodec opus -strict -2 -f rtp rtp://10.33.250.241:8002
```

## 扬声器推流音频opus编码

扬声器采集得下载采集软件 screen capture recorder。  
官网链接：[http://sourceforge.net/projects/screencapturer/files/](http://sourceforge.net/projects/screencapturer/files/)

```bash
ffmpeg -f dshow -i audio="virtual-audio-capturer" -vn -acodec copy -acodec opus -strict -2 -f rtp rtp://10.33.250.241:8002
```

## 扬声器推流音频opus编码并设置时间戳调整降低延迟。

```bash
ffmpeg -f dshow  -itsoffset -0.1 -i audio="virtual-audio-capturer" -vn -acodec copy -acodec opus -strict -2 -f  rtp rtp://10.33.250.241:8002
```

## 采集屏幕编码并推流

```bash
 ffmpeg -rtbufsize 2000M -f dshow  -i video="screen-capture-recorder" -vcodec copy -vcodec nvenc_h264 -zerolatency 1 -f rtp rtp://10.33.250.241:8004   
```

# 低延迟设置

以下参数为降低延迟设置，一个个调整花费了我大量时间。

## \-rtbufsize 1000k

设置缓冲区大小，太小会爆缓存，并且引起丢帧，而且容易引起传输卡死，太大没有明显影响

## \-audio\_buffer\_size 1k

设置一秒内音频处理包缓存大小，太小会导致同一个时间段音频数据分为多个数据包处理导致延迟加大，太大会设置失败，如果给流加上此设置貌似不起作用

## \-itsoffset -0.1

//设置时间戳向左偏移，如果不是储存视频文件，或者视频文件推流貌似不起效果

## \-max\_muxing\_queue\_size 0

设置最大处理音频流的数量

## \-bufsize 0

设置单一处理音频流的大小，以上设置结合如下：

```bash
ffmpeg -f dshow -rtbufsize 1000M -audio_buffer_size 1k -itsoffset 0.08 -i audio="virtual-audio-capturer" -vn -audio_buffer_size 1 -max_muxing_queue_size 1 -bufsize 1  -acodec copy -acodec libopus -ar 48000 -strict -2 -b:a 64K -f  rtp rtp://10.33.250.241:8002
```

## \-itsscale 1

设置输入数据速度，坑点，此参数不当会引起视频流的延迟，最后的设置如下：

```bash
ffmpeg -f dshow -rtbufsize 1000M -audio_buffer_size 1k -itsoffset 0.08 -itsscale 1 -i audio="virtual-audio-capturer" -vn -ss 0.08 -audio_buffer_size 1 -max_muxing_queue_size 1 -bufsize 1  -acodec copy -acodec libopus -ar 48000 -strict -2 -b:a 64K -f  rtp rtp://10.33.250.241:8002
```

## 先推视频再推音频超低延迟的另一种设置，测试通过

```bash
ffmpeg  -f dshow -rtbufsize 4.5k -itsscale 1 -itsoffset -0.1 -i audio="virtual-audio-capturer"
```