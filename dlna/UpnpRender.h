#pragma once
#include <memory>
#include <string>
#include <functional>
#include "UpnpServer.h"

class UpnpRender : public std::enable_shared_from_this<UpnpRender>
{
	Device::Ptr model_;
	std::string url_;
	float duration_;
	float speed_ = 1.0f;
public:
	typedef std::shared_ptr<UpnpRender> Ptr;
	UpnpRender(Device::Ptr dev) :model_(dev) {}
	~UpnpRender();
	float duration() const { return duration_; }
	float speed() const { return speed_; }

	/**
	设置投屏地址
	@param urlStr 视频url
	*/
	int setAVTransportURL(const char* urlStr, RpcCB cb);
	std::string getURL() const { return url_; }
	float getDuration() const { return duration_; }
	/**
	播放
	*/
	int play(float speed = 1.0f, RpcCB cb = nullptr);

	/**
	暂停
	*/
	int pause(RpcCB cb = nullptr);

	/**
	结束
	*/
	int stop(RpcCB cb = nullptr);


	/**
	跳转进度
	@param relTime 进度时间(单位秒)
	*/
	int seek(float relTime, RpcCB cb = nullptr);

	/**
	跳转至特定进度或视频
	@param target 目标值，可以是 00:02:21 格式的进度或者整数的 TRACK_NR。
	@param unit   REL_TIME（跳转到某个进度）或 TRACK_NR（跳转到某个视频）。
	*/
	int seekToTarget(const char* target, const char* unit, RpcCB cb);

	/**
	获取播放进度,可通过协议回调使用
	*/
	int getPositionInfo(std::function<void(int, AVPositionInfo)> cb);

	/**
	获取播放状态,可通过协议回调使用
	*/
	int getTransportInfo(std::function<void(int, TransportInfo)> cb);

	/**
	获取音频,可通过协议回调使用
	*/
	int getVolume(std::function<void(int, int)> cb);
	// 0-100
	int setVolume(int value, RpcCB cb = nullptr);

	/**
	设置音频值
	@param value 值—>整数
	*/
	int setVolumeWith(const char* value, RpcCB cb = nullptr);

};
