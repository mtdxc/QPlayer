#include "UpnpServer.h"
#include <strstream>
#include "hstring.h"
#include "pugixml.hpp"
#include "UpnpRender.h"

// httplib
#include "httplib.h"
using namespace httplib;

#ifndef WIN32
typedef int SOCKET;
#define closesocket ::close
#endif

static std::string socket_errstr() {
	char buffer[128] = { 0 };
#ifdef _WIN32
	int err = WSAGetLastError();
	int n = snprintf(buffer, sizeof(buffer), "%d:", err);
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_MAX_WIDTH_MASK,
		0, err, 0, buffer + n, sizeof(buffer) - n, NULL);
	return buffer;
#else
	int err = errno;
	snprintf(buffer, sizeof(buffer), "%d:%s", err, strerror(err));
#endif
	return buffer;
}

const char* ssdpAddres = "239.255.255.250";
const unsigned short ssdpPort = 1900;
typedef std::map<std::string, std::string, hv::StringCaseLess> http_headers;
const char* getServiceTypeStr(UpnpServiceType t) {
	switch (t)
	{
#define XX(type, id, str) case type : return str;
		SERVICE_MAP(XX)
#undef XX
default:
	return "";
	}
}

const char* getServiceIdStr(UpnpServiceType t) {
	switch (t)
	{
#define XX(type, id, str) case type : return id;
		SERVICE_MAP(XX)
#undef XX
default:
	return "";
	}
}

UpnpServiceType getServiceType(const std::string& str)
{
#define XX(type, id, str) {str, type},
	static std::map<std::string, UpnpServiceType, hv::StringCaseLess> codec_map = { SERVICE_MAP(XX) };
#undef XX
	auto it = codec_map.find(str);
	return it == codec_map.end() ? USInvalid : it->second;
}

UpnpServiceType getServiceId(const std::string& str)
{
#define XX(type, id, str) {id, type},
	static std::map<std::string, UpnpServiceType, hv::StringCaseLess> codec_map = { SERVICE_MAP(XX) };
#undef XX
	auto it = codec_map.find(str);
	return it == codec_map.end() ? USInvalid : it->second;
}

bool parseUrl(const std::string& url, std::string& host, std::string& path)
{
	int pos = url.find("://");
	if (pos == -1)
		pos = 0;
	else
		pos += 3;
	pos = url.find('/', pos);
	host = url.substr(0, pos);
	if (pos != -1) {
		path = url.substr(pos);
	}
	return !host.empty();
}

void Device::set_location(const std::string& loc)
{
	this->location = loc;
	if (URLHeader.empty()) {
		auto pos = location.find("://");
		if (pos != -1) {
			pos += 3;
			pos = location.find('/', pos);
		}
		else {
			pos = location.find('/');
		}
		if (pos != -1) {
			URLHeader = location.substr(0, pos);
		}
	}
}

std::string Device::getPostUrl(UpnpServiceType t) const {
	auto model = getService(t);
	if (!model || model->controlURL.empty())
		return "";
	std::string ret = URLHeader;
	if (model->controlURL[0] != '/')
		ret += "/";
	return ret + model->controlURL;
}

ServiceModel::Ptr Device::getService(UpnpServiceType t) const
{
	auto it = services_.find(t);
	if (it == services_.end())
		return nullptr;
	return it->second;
}

std::string Device::description() const
{
	std::ostringstream stm;
	stm << "\nuuid:" << uuid
		<< "\nlocation:" << location
		<< "\nURLHeader:" << URLHeader
		<< "\nfriendlyName:" << friendlyName
		<< "\nmodelName:" << modelName
		<< "\n";
	return stm.str();
}

const char* Upnp::getUrlPrefix()
{
	if (!url_prefix_[0])
		DetectLocalIP();
	return url_prefix_;
}

std::string Upnp::getUrl(const char* loc)
{
	std::string ret(url_prefix_);
	if (loc && loc[0]) {
		if (loc[0] != '/')
			ret += '/';
		ret += loc;
	}
	return ret;
}

// 本地文件共享
std::string Upnp::setFile(const char* path, const char* loc) {
	std::string ret;
	if (!loc || !loc[0]) {
		loc = strrchr(path, '/');
		if (!loc) loc = strrchr(path, '\\');
		if (!loc || !loc[0]) return ret;
	}
	if (path && path[0]) {
		file_maps_[loc] = path;
		ret = getUrl(loc);
	}
	else {
		file_maps_.erase(loc);
	}
	return ret;
}

void Upnp::addListener(UpnpListener* l)
{
	auto it = std::find(_listeners.begin(), _listeners.end(), l);
	if (it == _listeners.end()) {
		_listeners.push_back(l);
	}
}

void Upnp::delListener(UpnpListener* l)
{
	auto it = std::find(_listeners.begin(), _listeners.end(), l);
	if (it != _listeners.end()) {
		_listeners.erase(it);
	}
}

Upnp* Upnp::Instance()
{
	static Upnp upnp;
	return &upnp;
}

Upnp::~Upnp()
{
	stop();
}

httplib::TaskQueue* Upnp::getTaskQueue()
{
	return _svr ? _svr->getTaskQueue() : nullptr;
}

void Upnp::DetectLocalIP()
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("8.8.8.8");
	addr.sin_port = htons(553);
	int ret = connect(sock, (sockaddr*)&addr, sizeof(addr));
	if (ret == -1){
		printf("connect error %d\n", ret);
		closesocket(sock);
		return;
	}
	socklen_t len = sizeof(addr);
	getsockname(sock, (sockaddr*)&addr, &len);
	char buf[65] = { 0 };
	inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
	closesocket(sock);
	sprintf(url_prefix_, "http://%s:%d", buf, local_port_);
	Output("got url_prefix: %s", url_prefix_);
}

void Upnp::UdpReadFunc()
{
	char buff[1600] = { 0 };
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	while (_socket != INVALID_SOCKET)
	{
		socklen_t addrlen = sizeof(addr);
		int len = recvfrom(_socket, buff, sizeof(buff), 0, (sockaddr*)&addr, &addrlen);
		if (len > 0)
			onUdpRecv(buff, len);
	}
}

void Upnp::start()
{
	if (_socket != INVALID_SOCKET) return;
#ifdef WIN32
	WSAData wsad;
	WSAStartup(MAKEWORD(2, 0), &wsad);
#endif
	_socket = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	int ret = bind(_socket, (sockaddr*)&addr, sizeof(addr));
	if (0 != ret) {
		Output("bind error %s", socket_errstr().c_str());
		return;
	}

	ip_mreq m_membership;
	m_membership.imr_multiaddr.s_addr = inet_addr(ssdpAddres);
	m_membership.imr_interface.s_addr = htons(INADDR_ANY);
	ret = setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&m_membership, sizeof(m_membership));
	if (0 != ret)
	{
		Output("IP_ADD_MEMBERSHIP error %s", socket_errstr().c_str());
		//stop();
		return;
	}

	udp_thread_ = std::thread(&Upnp::UdpReadFunc, this);

	_svr = new Server;
	_svr->set_pre_routing_handler([this](const Request& req, Response& res) {
		auto it = file_maps_.find(req.path);
		if (it == file_maps_.end()) {
			return Server::HandlerResponse::Unhandled;
		}
		std::string path = it->second;
		FILE* fp = fopen(path.c_str(), "rb");
		if (!fp) {
			res.status = 404;
			res.reason = "file not found";
			return Server::HandlerResponse::Handled;
		}
		fseek(fp, 0, SEEK_END);
		size_t size = ftell(fp);
		const char* content_type = detail::find_content_type(path, {});
		res.set_content_provider(size, content_type, [fp](size_t offset, size_t length, DataSink& sink) {
			char buff[4096];
			fseek(fp, offset, SEEK_SET);
			int n = sizeof(buff);
			if (length < n)
				n = length;
			n = fread(buff, 1, n, fp);
			sink.write(buff, n);
			return true; // return 'false' if you want to cancel the process.
		}, [fp](bool success) {
			fclose(fp);
		});
		return Server::HandlerResponse::Handled;
	});

	/*
	_svr->set_logger([](const Request &req, const Response &res) {
		printf("%s", log(req, res).c_str());
	});
	*/
	local_port_ = _svr->bind_to_any_port("0.0.0.0");
	std::thread([this] {
		_svr->listen_after_bind();
	}).detach();
	DetectLocalIP();
}

void Upnp::stop()
{
	if (_socket != INVALID_SOCKET) {
		// @note 必须先shutdown，否则 android 的 udp_thread 退不出
#ifdef _WIN32
		shutdown(_socket, SD_BOTH);
#else
		shutdown(_socket, SHUT_RDWR);
#endif
		closesocket(_socket);
		_socket = INVALID_SOCKET;
	}
	if (udp_thread_.joinable())
		udp_thread_.join();
	_devices.clear();
	_renders.clear();
	if (_svr) {
		_svr->stop();
		_svr = nullptr;
	}
}

void Upnp::search()
{
	_devices.clear();
	onChange();

	char line[1024];
	int size = sprintf(line, "M-SEARCH * HTTP/1.1\r\nHOST: %s:%d\r\nMAN: \"ssdp:discover\"\r\nMX: 3\r\nST: %s\r\nUSER-AGENT: iOS UPnP/1.1 Tiaooo/1.0\r\n\r\n",
		ssdpAddres, ssdpPort, getServiceTypeStr(USAVTransport));
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ssdpAddres);
	addr.sin_port = htons(ssdpPort);
	int ret = sendto(_socket, line, size, 0, (sockaddr*)&addr, sizeof(addr));
	if (ret != size) {
		Output("sendto %d return %d error %s", size, ret, socket_errstr().c_str());
	}
}

Device::Ptr Upnp::getDevice(const char* usn)
{
	auto it = _devices.find(usn);
	if (it != _devices.end()) {
		return it->second;
	}
	return nullptr;
}

UpnpRender::Ptr Upnp::getRender(const char* usn)
{
	UpnpRender::Ptr ret;
	auto it = _renders.find(usn);
	if (_renders.end() == it) {
		auto dev = getDevice(usn);
		if (!dev)
			return nullptr;
		ret = std::make_shared<UpnpRender>(dev);
		_renders[usn] = ret;
	}
	else {
		ret = it->second;
	}
	return ret;
}

int Upnp::openUrl(const char* id, const char* url, RpcCB cb)
{
	int ret = 0;
	if (auto render = getRender(id))
		ret = render->setAVTransportURL(url, cb);
	return ret;
}

int Upnp::pause(const char* id, RpcCB cb)
{
	int ret = 0;
	if (auto render = getRender(id))
		ret = render->pause(cb);
	return ret;
}

int Upnp::resume(const char* id, RpcCB cb)
{
	int ret = 0;
	if (auto render = getRender(id))
		ret = render->play(render->speed(), cb);
	return ret;
}

int Upnp::seek(const char* id, float val, RpcCB cb)
{
	int ret = 0;
	if (auto render = getRender(id))
		ret = render->seek(val, cb);
	return ret;
}

int Upnp::setVolume(const char* id, int val, RpcCB cb)
{
	int ret = 0;
	if (auto render = getRender(id))
		ret = render->setVolume(val, cb);
	return ret;
}

int Upnp::getVolume(const char* id, std::function<void(int, int)> cb)
{
	int ret = 0;
	if (auto render = getRender(id))
		ret = render->getVolume(cb);
	return ret;
}

int Upnp::getPosition(const char* id, std::function<void(int, AVPositionInfo)> cb)
{
	int ret = 0;
	if (auto render = getRender(id))
		ret = render->getPositionInfo(cb);
	return ret;
}

int Upnp::getInfo(const char* id, std::function<void(int, TransportInfo)> cb)
{
	int ret = 0;
	if (auto render = getRender(id))
		ret = render->getTransportInfo(cb);
	return ret;
}

int Upnp::setSpeed(const char* id, float speed, RpcCB cb)
{
	int ret = 0;
	if (auto render = getRender(id))
		ret = render->play(speed, cb);
	return ret;
}

float Upnp::getSpeed(const char* id)
{
	float ret = 1.0f;
	if (auto render = getRender(id))
		ret = render->speed();
	return ret;
}

float Upnp::getDuration(const char* id)
{
	float ret = 0.0f;
	if (auto render = getRender(id))
		ret = render->duration();
	return ret;
}

int Upnp::close(const char* id, RpcCB cb)
{
	int ret = 0;
	auto it = _renders.find(id);
	if (_renders.end() != it) {
		auto render = it->second;
		_renders.erase(it);
		if (render) {
			ret = render->stop(cb);
		}
	}
	return ret;
}

void parseHttpHeader(char* buff, int size, http_headers& header, std::string* fline) {
	int nline = 0;
	char line[512] = { 0 };
	std::istrstream resp_stm(buff, size);
	while (resp_stm.getline(line, sizeof line)) {
		if (nline++ == 0) {
			if (fline)
				*fline = line;
			int temp_pos;
			unsigned int vmajor, vminor, temp_scode;
			if (sscanf(line, "HTTP %u%n",
				&temp_scode, &temp_pos) == 1) {
				// This server's response has no version. :( NOTE: This happens for every
				// response to requests made from Chrome plugins, regardless of the server's
				// behaviour.
			}
			else if ((sscanf(line, "HTTP/%u.%u %u%n",
				&vmajor, &vminor, &temp_scode, &temp_pos) == 3)
				&& (vmajor == 1)) {
				// This server's response does have a version.
			}
			else {

			}
			//resp_code = temp_scode;
		}
		else {
			char* p = ::strchr(line, ':');
			if (p) {
				*p++ = 0;
				std::string key = hv::trim(line);
				if (key.length())
					header[key] = hv::trim(p);
			}
		}
	}
}

void Upnp::onUdpRecv(char* buff, int size)
{
	char* p = buff;
	// printf("udp> %.*s", buffer->size(), p);
	if (!strncasecmp(p, "NOTIFY", 6)) {
		http_headers header;
		parseHttpHeader(buff, size, header, nullptr);
		auto serviceType = getServiceType(header["NT"].c_str());
		if (serviceType == USAVTransport) {
			std::string location = header["Location"];
			std::string usn = header["USN"];
			std::string ssdp = header["NTS"];
			if (location.empty() || usn.empty() || ssdp.empty()) {
				//LOG(INFO) << "skip";
				return;
			}
			if (ssdp == "ssdp:alive") {
				if (!_devices.count(usn)) {
					loadDeviceWithLocation(location, usn);
				}
			}
			else if (ssdp == "ssdp:byebye") {
				delDevice(usn);
			}
		}
	}
	else if (!strncasecmp(p, "HTTP/1.1", 8)) {
		http_headers header;
		parseHttpHeader(buff, size, header, nullptr);
		std::string location = header["Location"];
		std::string usn = header["USN"];
		if (location.empty() || usn.empty()) {
			return;
		}
		loadDeviceWithLocation(location, usn);
	}
}

void Upnp::delDevice(const std::string& usn)
{
	auto it = _devices.find(usn);
	if (it != _devices.end()) {
		_devices.erase(it);
	}
	onChange();
}

void Upnp::loadDeviceWithLocation(std::string loc, std::string usn)
{
	getTaskQueue()->enqueue([=]() {
		std::string host, path;
		parseUrl(loc, host, path);
		Client http(host);
		Result res = http.Get(path);
		if (!res || res->status != 200)
			return;

		pugi::xml_document doc;
		doc.load_string(res->body.c_str());
		auto root = doc.first_child();
		auto dev = root.child("device");
		if (!dev) return;
		Device::Ptr device = std::make_shared<Device>();
		device->location = loc;
		if (dev.child("UDN")) {
			device->uuid = dev.child("UDN").child_value();
		}
		else {
			auto pos = usn.find("::");// "::urn:");
			if (-1 != pos && pos > 0)
				device->uuid = usn.substr(0, pos);
			else
				device->uuid = usn;
		}

		device->friendlyName = dev.child("friendlyName").child_value();
		device->modelName = dev.child("modelName").child_value();
		for (auto service : dev.child("serviceList").children("service")) {
			/*
			<service>
			<serviceType>urn:schemas-upnp-org:service:AVTransport:1</serviceType>
			<serviceId>urn:upnp-org:serviceId:AVTransport</serviceId>
			<SCPDURL>/upnp/rendertransportSCPD.xml</SCPDURL>
			<controlURL>/upnp/control/rendertransport1</controlURL>
			<eventSubURL>/upnp/event/rendertransport1</eventSubURL>
			</service>
			*/
			auto sm = std::make_shared<ServiceModel>();
			sm->serviceType = service.child("serviceType").child_value();
			sm->serviceId = service.child("serviceId").child_value();
			sm->controlURL = service.child("controlURL").child_value();
			sm->eventSubURL = service.child("eventSubURL").child_value();
			sm->SCPDURL = service.child("SCPDURL").child_value();
			auto type1 = getServiceId(sm->serviceId);
			auto type2 = getServiceType(sm->serviceType);
			if (type1 != USInvalid) {
				device->services_[type1] = sm;
			}
			else if (type2 != USInvalid) {
				device->services_[type2] = sm;
			}
		}
		if (auto url = root.child("URLBase")) {
			device->URLHeader = url.child_value();
			int len = device->URLHeader.length() - 1;
			if (len && device->URLHeader[len - 1] == '/')
				device->URLHeader[len - 1] = 0;
		}
		device->set_location(loc);
		addDevice(device);
	});
}

void Upnp::addDevice(Device::Ptr device)
{
	Output("%s", device->description().c_str());
	_devices[device->uuid] = device;
	onChange();
}

void Upnp::onChange() {
	for (auto l : _listeners) {
		l->upnpSearchChangeWithResults(_devices);
	}
}
