#pragma once
#include <map>
#include <string>
#include <list>
#include <functional>
#include <memory>

class OnvifDevice : public std::enable_shared_from_this<OnvifDevice>
{
	bool getMediaUrl(std::string& nsp, std::string& url);
public:
	typedef std::function<void(int, std::string)> RpcCB;
	OnvifDevice(const char* id, const char* purl);
	~OnvifDevice();

	void GetServices(bool incCapability, RpcCB cb);
	void GetProfiles(bool useCache, RpcCB cb);
	void GetStreamUri(const std::string& profile, RpcCB cb);
	void GetDeviceInformation(RpcCB cb);
	std::string uuid;
	std::string devUrl;
	std::string name;
	std::map<std::string, std::string> services;
	std::map<std::string, std::string> profiles;
};

