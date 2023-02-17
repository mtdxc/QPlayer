#pragma once
#include <map>
#include <string>
#include <list>
#include <functional>
#include <memory>

/*
<tds:SetNetworkInterfaces>
  <tds:InterfaceToken>InterfaceToken1</tds:InterfaceToken>
  <tds:NetworkInterface>
    <tt:Enabled>true</tt:Enabled>
    <tt:Link>
      <tt:AutoNegotiation>true</tt:AutoNegotiation>
      <tt:Speed>10</tt:Speed>
      <tt:Duplex>Full</tt:Duplex>
    </tt:Link>
    <tt:MTU>1</tt:MTU>
    <tt:IPv4>
      <tt:Enabled>true</tt:Enabled>
      <tt:Manual>
        <tt:Address>192.168.10.42</tt:Address>
        <tt:PrefixLength>24</tt:PrefixLength>
      </tt:Manual>
      <tt:DHCP>true</tt:DHCP>
    </tt:IPv4>
  </tds:NetworkInterface>
</tds:SetNetworkInterfaces>
*/

struct NetworkInterface {
	std::string ip;
};

struct DeviceInformation {
	std::string Manufacturer, Model, FirmwareVersion, SerialNumber, HardwareId;
};

struct OnvifUrl {
	std::string media, snap;
};

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
		//eExtension
	};
	static const char* CatalogName(Catalog cat);
	static Catalog CatalogByName(const char* name);

	typedef std::function<void(int, std::string)> RpcCB;
	OnvifDevice(const char* id, const char* purl);
	~OnvifDevice();

	void GetCapabilities(Catalog cata = eAll, RpcCB cb = nullptr);
	void GetServices(bool incCapability, RpcCB cb);
	void GetProfiles(bool useCache, RpcCB cb);

	// ffplay rtsp://100.100.100.5:554/av0_0
	// ffplay rtsp://username:password@100.100.100.5:554/av0_0
	void GetStreamUri(const std::string& profile, RpcCB cb);

	// wget -O out.jpeg 'http://100.100.100.5:80/capture/webCapture.jpg?channel=1&FTpsend=0&checkinfo=0'
	// wget -O out.jpeg 'http://username:password@100.100.100.5:80/capture/webCapture.jpg?channel=1&FTpsend=0&checkinfo=0'
	void GetSnapshotUri(const std::string& profile, RpcCB cb);

	void GetDeviceInformation(std::function<void(int, DeviceInformation)> cb);

	void GetNetworkInterfaces(std::function<void(int, const NetworkInterface& in)> cb);
	void SetNetworkInterfaces(const NetworkInterface& in, RpcCB cb);

	std::string uuid;
	std::string devUrl;
	std::string name;
	// catalog -> url
	std::map<Catalog, std::string> services;
	// profile -> url
	std::map<std::string, OnvifUrl> profiles;
};

