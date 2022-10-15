#pragma once
#include <map>
#include <memory>
#include <string>
#include <list>
#include <thread>
#include <functional>
namespace httplib {
	class Server;
	class TaskQueue;
}

class UpnpRender;
enum UpnpServiceType {
	USInvalid = -1,
	USAVTransport,
	USRenderingControl,
};
#define SERVICE_MAP(XX) \
    XX(USAVTransport, "urn:upnp-org:serviceId:AVTransport", "urn:schemas-upnp-org:service:AVTransport:1") \
    XX(USRenderingControl, "urn:upnp-org:serviceId:RenderingControl", "urn:schemas-upnp-org:service:RenderingControl:1")  \

const char* getServiceTypeStr(UpnpServiceType t);
const char* getServiceIdStr(UpnpServiceType t);
UpnpServiceType getServiceType(const std::string& p);
UpnpServiceType getServiceId(const std::string& p);
void Output(const char* fmt, ...);
bool parseUrl(const std::string& url, std::string& host, std::string& path);
struct ServiceModel {
	std::string serviceType, serviceId;
	std::string controlURL, eventSubURL, SCPDURL;
	typedef std::shared_ptr<ServiceModel> Ptr;
};

struct Device {
	std::string uuid;
	std::string location, URLHeader;
	std::string friendlyName;
	std::string modelName;

	std::map<UpnpServiceType, ServiceModel::Ptr> services_;
	void set_location(const std::string& loc);
	std::string description() const;
	std::string getPostUrl(UpnpServiceType t) const;
	ServiceModel::Ptr getService(UpnpServiceType t) const;
	typedef std::shared_ptr<Device> Ptr;
};
typedef std::map<std::string, Device::Ptr> MapDevices;

struct AVPositionInfo {
	float trackDuration;
	float absTime;
	float relTime;
	AVPositionInfo() { trackDuration = absTime = relTime = 0.0f; }
};

struct TransportInfo {
	char state[32];
	char status[32];
	float speed;
	TransportInfo();
	void setState(const char* v);
	void setStatus(const char* v);
};
typedef std::function<void(int, std::string)> RpcCB;

class UpnpListener {
public:
	virtual void upnpSearchChangeWithResults(const MapDevices& devs) = 0;
	// 调用UPnPAction时,会返回一个id, 可通过hook此方法来获取所有soap调用的返回值
	virtual void unppActionResponse(int id, int code, const std::map<std::string, std::string>& args) {}
};

class Upnp
{
	MapDevices _devices; //发现的设备
	std::map<std::string, std::shared_ptr<UpnpRender>> _renders; // 连接的渲染器
	// dlna发现套接口
	int _socket = -1;
	std::thread udp_thread_;
	void UdpReadFunc();
	void onUdpRecv(char* buf, int size);

	void loadDeviceWithLocation(std::string loc, std::string usn);
	void addDevice(Device::Ptr device);
	void delDevice(const std::string& usn);
	void onChange();

	httplib::Server* _svr = nullptr;
	std::list<UpnpListener*> _listeners;

	int local_port_ = 0;
	char url_prefix_[128];
	std::map<std::string, std::string> file_maps_;
	void DetectLocalIP();
	Upnp() = default;
	Device::Ptr getDevice(const char* usn);
	std::shared_ptr<UpnpRender> getRender(const char* usn);
public:
	const char* getUrlPrefix();
	// return local_uri_ + "/" + loc
	std::string getUrl(const char* path);

	// 本地文件共享
	std::string setFile(const char* path, const char* loc = nullptr);
	// 保证旧版兼容
	std::string setCurFile(const char* path) {
		return setFile(path, "/media");
	}
	std::string getCurUrl() {
		return getUrl("/media");
	}

	void addListener(UpnpListener* l);
	void delListener(UpnpListener* l);
	// used for UpnpRender
	const std::list<UpnpListener*>& getListeners() { return _listeners; }
	httplib::TaskQueue* getTaskQueue();

	static Upnp* Instance();
	~Upnp();

	void start();
	void stop();

	// 搜索设备
	void search();
	// 返回设备列表
	const MapDevices& getDevices() { return _devices; }

	// 代理 AVTransport API
	int openUrl(const char* id, const char* url, RpcCB cb = nullptr);
	int close(const char* id, RpcCB cb = nullptr);
	int pause(const char* id, RpcCB cb = nullptr);
	int resume(const char* id, RpcCB cb = nullptr);
	int seek(const char* id, float val, RpcCB cb = nullptr);
	int getPosition(const char* id, std::function<void(int, AVPositionInfo)> cb = nullptr);
	int getInfo(const char* id, std::function<void(int, TransportInfo)> cb = nullptr);
	int setSpeed(const char* id, float speed, RpcCB cb = nullptr);
	float getSpeed(const char* id);
	float getDuration(const char* id);
	// RenderingControl api
	int setVolume(const char* id, int val, RpcCB cb = nullptr);
	int getVolume(const char* id, std::function<void(int, int)> cb = nullptr);
};
