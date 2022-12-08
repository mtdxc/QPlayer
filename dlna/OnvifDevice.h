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
	enum Catalog {
		eAll,
		eAnalytics,
		eDevice,
		eEvents,
		eImage,
		eMedia,
		ePTZ
	};
	static const char* CatalogName(Catalog cat);
	static Catalog CatalogByName(const char* name);

	typedef std::function<void(int, std::string)> RpcCB;
	OnvifDevice(const char* id, const char* purl);
	~OnvifDevice();

	void GetCapabilities(Catalog cata = eAll, RpcCB cb = nullptr);
	void GetServices(bool incCapability, RpcCB cb);
	void GetProfiles(bool useCache, RpcCB cb);
	void GetStreamUri(const std::string& profile, RpcCB cb);
	void GetDeviceInformation(RpcCB cb);
	std::string uuid;
	std::string devUrl;
	std::string name;
	// catalog -> url
	std::map<Catalog, std::string> services;
	// profile -> url
	std::map<std::string, std::string> profiles;
};

