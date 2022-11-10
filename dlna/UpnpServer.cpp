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

std::string concatUrl(const std::string& prefix, const std::string& tail) {
	if (prefix.empty())
		return tail;
	if (tail.empty())
		return prefix;
	if (*prefix.rbegin() != '/' && *tail.begin() != '/')
		return prefix + "/" + tail;
	return prefix + tail;
}

bool ServiceModel::parseScpd(const std::string& baseUrl)
{
	if (scpdURL.empty() || baseUrl.empty())
		return false;

	std::string host, path, url = concatUrl(baseUrl, scpdURL);
	parseUrl(url, host, path);
	Client http(host);
	Result res = http.Get(path);
	if (!res || res->status != 200)
		return false;

	this->actions_.clear();
	this->stateVals_.clear();
	pugi::xml_document doc;
	doc.load_string(res->body.c_str());
	auto root = doc.first_child(); // scpd
	auto actions = root.select_nodes("actionList/action");
	Output("%s got %d actions", url.c_str(), actions.size());
	for (int j = 0; j < actions.size(); j++) {
		/* <action><name>SelectPreset</name><argumentList><argument/>...</argumentList></action> */
		auto action = actions[j].node();
		auto action_name = action.child("name").child_value();
		Output("action %d> %s", j, action_name);
		auto& act = this->actions_[action_name];
		auto args = action.select_nodes("argumentList/argument");
		for (int i = 0; i < args.size(); i++) {
			/*
			<argument>
				<name>InstanceID</name>
				<direction>in</direction>
				<relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
			</argument>
			*/
			auto arg = args[i].node();
			auto dir = arg.child("direction").child_value();
			auto name = arg.child("name").child_value();
			auto ref = arg.child("relatedStateVariable").child_value();
			Output(" [%d] %s %s ref=%s", i, dir, name, ref);
			act[name] = std::string(strcasecmp(dir, "in") ? "0" : "1") + ref;
		}
	}

	auto vals = root.select_nodes("serviceStateTable/stateVariable");
	Output("%s got %d stateVariables", url.c_str(), vals.size());
	for (int i = 0; i < vals.size(); i++)
	{
		auto val = vals[i].node();
		/*
		<stateVariable sendEvents="no">
			<name>GreenVideoGain</name>
			<dataType>ui2</dataType>
			<allowedValueRange>
				<minimum>0</minimum>
				<maximum>100</maximum>
				<step>1</step>
			</allowedValueRange>
		</stateVariable>
		*/
		auto name = val.child("name").child_value();
		auto type = val.child("dataType").child_value();
		Output(" [%d] %s %s", i, type, name);
		stateVals_[name] = type;
	}
	return true;
}

const char* ServiceModel::findActionArg(const char* name, const char* arg) const
{
	auto it = actions_.find(name);
	if (it != actions_.end()) {
		auto it2 = it->second.find(arg);
		if (it2 != it->second.end()) {
			return it2->second.c_str();
		}
	}
	return nullptr;
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

std::string Device::getControlUrl(UpnpServiceType t) const {
	auto service = getService(t);
	if (!service || service->controlURL.empty())
		return "";
	return concatUrl(URLHeader, service->controlURL);
}

std::string Device::getEventUrl(UpnpServiceType t) const
{
	auto service = getService(t);
	if (!service || service->eventSubURL.empty())
		return "";
	return concatUrl(URLHeader, service->eventSubURL);
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

void Upnp::addSidListener(const std::string& sid, UpnpSidListener* l)
{
	sid_maps_[sid] = l;
}

void Upnp::delSidListener(const std::string& sid)
{
	sid_maps_.erase(sid);
}

const char* Upnp::getUrlPrefix()
{
	if (!url_prefix_[0])
		DetectLocalIP();
	return url_prefix_;
}

std::string Upnp::getUrl(const char* loc)
{
	std::string ret(getUrlPrefix());
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
		Output("connect error %d", ret);
		closesocket(sock);
		return;
	}
	socklen_t len = sizeof(addr);
	ret = getsockname(sock, (sockaddr*)&addr, &len);
	if (ret == -1){
		Output("getsockname error %d", ret);
		return;
	}
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
		stop();
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
	_svr->set_pre_routing_handler([this](const Request& req, Response& res, Stream& stm) {
		if (req.method == "NOTIFY"){
			std::string len = req.get_header_value("Content-Length");
			if (len.length() && !_svr->read_content(stm, const_cast<Request&>(req), res)){
				res.status = 404;
				return Server::HandlerResponse::Handled;
			}
      /*
			NOTIFY delivery_path HTTP/1.1
			HOST:delivery_host:delivery_port
			CONTENT-TYPE:  text/xml
			CONTENT-LENGTH: Bytes in body
			NT: upnp:event
			NTS: upnp:propchange
			SID: uuid:subscription-UUID
			SEQ: event key

			<e:propertyset xmlns:e="urn:schemas-upnp-org:event-1-0">
			<e:property>
			<variableName>new value</variableName>
			<e:property>
			Other variable names and values (if any) go here.
			</e:propertyset>
			*/
			auto sid = req.get_header_value("SID");
			auto it = sid_maps_.find(sid);
			if (it!=sid_maps_.end()) {
				it->second->onSidMsg(sid, req.body);
			}
			res.status = 200;
			return Server::HandlerResponse::Handled;
		}

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
	_devices.clear();
	_renders.clear();
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

int Upnp::subscribe(const char* id, int type, int sec)
{
	int ret = 0;
	if (auto render = getRender(id))
		render->subscribe(type, sec);
	return ret;
}

int Upnp::unsubscribe(const char* id, int type)
{
	int ret = 0;
	if (auto render = getRender(id))
		render->unsubscribe(type);
	return ret;
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

void parseHttpHeader(char* buff, int size, http_headers& header) {
	int nline = 0;
	char line[512] = { 0 };
	std::istrstream resp_stm(buff, size);
	while (resp_stm.getline(line, sizeof line)) {
		if (nline++ == 0) {
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
		parseHttpHeader(buff, size, header);
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
		parseHttpHeader(buff, size, header);
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
	if (!getTaskQueue())
		return;
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
		if (auto url = root.child("URLBase")) {
			device->URLHeader = url.child_value();
			int len = device->URLHeader.length() - 1;
			if (len && device->URLHeader[len - 1] == '/')
				device->URLHeader[len - 1] = 0;
		}
		device->set_location(loc);
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
			sm->presentationURL = service.child("presentationURL").child_value();
			sm->scpdURL = service.child("SCPDURL").child_value();
			auto type = getServiceId(sm->serviceId);
			if (type != USInvalid) {
				type = getServiceType(sm->serviceType);
			}
			if (type != USInvalid) {
				device->services_[type] = sm;
				sm->parseScpd(device->URLHeader);
			}
		}
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
