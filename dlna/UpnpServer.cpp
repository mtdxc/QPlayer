#include "UpnpServer.h"
#include <strstream>
#include "hstring.h"
#include "UpnpRender.h"

#include "pugixml.hpp"
using namespace pugi;

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
const unsigned short onvifPort = 3702;

std::string CreateRandomUuid()
{
	static const char kHex[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
	static const char kUuidDigit17[4] = { '8', '9', 'a', 'b' };
	std::string str;
	uint8_t bytes[31];
	//RAND_bytes(bytes, 31);
	for (size_t i = 0; i < sizeof(bytes); i++) {
		bytes[i] = rand() % 255;
	}

	str.reserve(36);
	for (size_t i = 0; i < 8; ++i)
	{
		str.push_back(kHex[bytes[i] % 16]);
	}
	str.push_back('-');
	for (size_t i = 8; i < 12; ++i)
	{
		str.push_back(kHex[bytes[i] % 16]);
	}
	str.push_back('-');
	str.push_back('4');
	for (size_t i = 12; i < 15; ++i)
	{
		str.push_back(kHex[bytes[i] % 16]);
	}
	str.push_back('-');
	str.push_back(kUuidDigit17[bytes[15] % 4]);
	for (size_t i = 16; i < 19; ++i)
	{
		str.push_back(kHex[bytes[i] % 16]);
	}
	str.push_back('-');
	for (size_t i = 19; i < 31; ++i)
	{
		str.push_back(kHex[bytes[i] % 16]);
	}
	return str;
}

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

// 略过冒号前缀
const char* skipNsp(const char* name){
	const char* p = strchr(name, ':');
	if (p) return p + 1;
	return name;
}

namespace pugi {
	// 略过名字空间的查找
	bool ignorePrefixCompare(const char* name, const char* p){
		if (!strcasecmp(name, p)) return true;
		if (p = strchr(p, ':'))
			return !strcasecmp(name, p + 1);
		return false;
	}

	xml_attribute find_attr(const xml_node& parent, const char* name){
		return parent.find_attribute([name](const xml_attribute& attr){
			return ignorePrefixCompare(name, attr.name());
		});
	}
	const char* get_attr_val(const xml_node& parent, const char* name){
		auto attr = find_attr(parent, name);
		return attr.value();
	}

	// helper func: find child without nsp
	xml_node child_node(const xml_node& parent, const char* name){
		return parent.find_child([name](const xml_node& node){
			return ignorePrefixCompare(name, node.name());
		});
	}
	const char* child_text(const xml_node& node, const char* name){
		return child_node(node, name).text().as_string();
	}

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
	int count = 0;
	if (*prefix.rbegin() == '/')
		count++;
	if (*tail.begin() == '/')
		count++;
	switch (count)
	{
	case 0:
		return prefix + "/" + tail;
	case 2:
		return prefix + (tail.data() + 1);
	default:
		return prefix + tail;
	}
}

bool ServiceDesc::parseScpd(const std::string& url)
{
	if (url.empty())
		return false;

	std::string host, path;
	parseUrl(url, host, path);
	Client http(host);
	Result res = http.Get(path);
	if (!res || res->status != 200) {
		Output("unable to parse scpd from %s", url.c_str());
		return false;
	}

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
		<allowedValueList>
		<allowedValue>Master</allowedValue>
		<allowedValue>LF</allowedValue>
		<allowedValue>RF</allowedValue>
		</allowedValueList>
		</stateVariable>
		*/
		auto name = val.child("name").child_value();
		auto type = val.child("dataType").child_value();
		std::ostringstream stm;
		auto range = val.child("allowedValueRange");
		if (range){
			stm << "[" << range.child("minimum").child_value() << "-" << range.child("maximum").child_value()
				<< "/" << range.child("step").child_value() << "]";
		}
		else if (auto list = val.child("allowedValueList")) {
			stm << "[";
			for (auto node = list.first_child(); node; node = node.next_sibling()){
				stm << node.child_value() << ",";
			}
			stm << "]";
		}
		Output(" [%d] %s %s%s", i, name, type, stm.str().c_str());
		stateVals_[name] = type;
	}
	return true;
}

const char* ServiceDesc::findActionArg(const char* name, const char* arg) const
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

bool ServiceDesc::hasActionArg(const char* name, const char* arg, int dir) const
{
	if (auto v = findActionArg(name, arg)) {
		char d = dir + '0';
		return *v == d;
	}
	return false;
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

	// 设置广播
	int broadcast = 1;
	ret = setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));
	if (0 != ret) {
		Output("enable broadcast error", socket_errstr().c_str());
	}

	// 加入多播组
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

void Upnp::search(int type, bool use_cache)
{
	int oldSize = _devices.size();
	if (use_cache) {
		time_t now = time(nullptr);
		auto it = _devices.begin();
		while (it != _devices.end())
		{
			if ((now - it->second->lastTick) > DEVICE_TIMEOUT) {
				it = _devices.erase(it);
			}
			else {
				it++;
			}
		}
	}
	else {
		_devices.clear();
	}
	if (oldSize != _devices.size()) {
		onChange();
	}
	const char* sType = getServiceTypeStr((UpnpServiceType)type);
	if (!strlen(sType))
		sType = "upnp::rootdevice";
	char line[1024];
	int size = sprintf(line, "M-SEARCH * HTTP/1.1\r\nHOST: %s:%d\r\nMAN: \"ssdp:discover\"\r\nMX: 3\r\nST: %s\r\nUSER-AGENT: iOS UPnP/1.1 Tiaooo/1.0\r\n\r\n",
		ssdpAddres, ssdpPort, sType);
	Output("search with %s, use_cache=%d", sType, use_cache);
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

int Upnp::sendProbe(const char *types, const char* dstIp)
{
	//_onvifs.clear();
	if (!dstIp || !dstIp[0])
		dstIp = ssdpAddres;
	char Probe[4096];
	std::string mid = CreateRandomUuid();
	int size = sprintf(Probe, "<?xml version=\"1.0\" encoding=\"utf-8\"?>"\
		"<Envelope xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" xmlns=\"http://www.w3.org/2003/05/soap-envelope\">"\
		"<Header>"\
		"<wsa:MessageID xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">uuid:%s</wsa:MessageID>"\
		"<wsa:To xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>"\
		"<wsa:Action xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>"\
		"</Header>"\
		"<Body>"\
		"<Probe xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">"\
		"<Types>%s</Types>"\
		"<Scopes />"\
		"</Probe>"\
		"</Body>"\
		"</Envelope>\r\n", mid.c_str(), types);
	Output("sendProbe %s with %s content %s", dstIp, types, Probe);
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(dstIp);
	addr.sin_port = htons(onvifPort);
	int ret = sendto(_socket, Probe, size, 0, (sockaddr*)&addr, sizeof(addr));
	if (ret != size) {
		Output("sendto %d return %d error %s", size, ret, socket_errstr().c_str());
	}
	return ret;
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
		// @ todo 判断 lastTick?
		return it->second;
	}
	return nullptr;
}

OnvifPtr Upnp::getOnvif(const char* uuid)
{
	auto it = _onvifs.find(uuid);
	if (it != _onvifs.end()) {
		// @ todo 判断 lastTick?
		return it->second;
	}
	return nullptr;
}

void Upnp::addOnvif(OnvifPtr ptr)
{
	Output("addOnvif %s %s", ptr->uuid.c_str(), ptr->devUrl.c_str());
	_onvifs[ptr->uuid] = ptr;
	for (auto l : _listeners)
		l->onvifSearchChangeWithResults(_onvifs);
}

void Upnp::addOnvif(const char* url)
{
	auto ptr = std::make_shared<OnvifDevice>("", url);
	ptr->GetServices(false, [ptr, this](int code, std::string error){
		if (code) {
			Output("unable to get service %s error %d:%s", ptr->devUrl.c_str(), code, error.c_str());
			return;
		} 
		addOnvif(ptr);
	});
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
	// Output("udp> %.*s", size, buff);
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
	else if (!strncasecmp(p, "<?xml ", 6)) {
		/* <SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:SOAP-ENC="http://www.w3.org/2003/05/soap-encoding" xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsdd="http://schemas.xmlsoap.org/ws/2005/04/discovery" xmlns:tdn="http://www.onvif.org/ver10/network/wsdl" >
		<SOAP-ENV:Header>
			<wsa:MessageID>uuid:000f2795-2795-34d9-95d9-f6700003c613</wsa:MessageID>
			<wsa:RelatesTo>uuid:9b6bc931-bfff-4cc0-8e4d-165d95e1535c</wsa:RelatesTo>
			<wsa:To SOAP-ENV:mustUnderstand="true">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:To>
			<wsa:Action SOAP-ENV:mustUnderstand="true">http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</wsa:Action>
		</SOAP-ENV:Header>
		<SOAP-ENV:Body><wsdd:ProbeMatches><wsdd:ProbeMatch>
			<wsa:EndpointReference><wsa:Address>urn:uuid:000f2795-2795-34d9-95d9-f6700003c613</wsa:Address></wsa:EndpointReference>
			<wsdd:Types>tdn:NetworkVideoTransmitter</wsdd:Types>
			<wsdd:Scopes>onvif://www.onvif.org/type/Network_Video_Transmitter onvif://www.onvif.org/type/video_encoder onvif://www.onvif.org/type/audio_encoder onvif://www.onvif.org/type/ptz onvif://www.onvif.org/Profile/Streaming onvif://www.onvif.org/hardware/PA4 onvif://www.onvif.org/name/PA4</wsdd:Scopes>
			<wsdd:XAddrs>http://192.168.27.97:80/onvif/device_service</wsdd:XAddrs>
			<wsdd:MetadataVersion>1</wsdd:MetadataVersion>
		</wsdd:ProbeMatch></wsdd:ProbeMatches></SOAP-ENV:Body>
		</SOAP-ENV:Envelope> */
		pugi::xml_document doc;
		doc.load_string(p);

		bool del = false;
		auto body = child_node(doc.first_child(), "Body");
		auto match = child_node(child_node(body, "ProbeMatches"), "ProbeMatch");
		if (!match)
			match = child_node(body, "Hello");
		if (!match){
			match = child_node(body, "Bye");
			del = match;
		}
		if (match){
			auto uuid = child_text(child_node(match, "EndpointReference"), "Address");
			if (del){
				_onvifs.erase(uuid);
				for (auto l : _listeners)
					l->onvifSearchChangeWithResults(_onvifs);
				return;
			}
			auto addr = child_text(match, "XAddrs");
			auto scopes = child_text(match, "Scopes");
			auto type = child_text(match, "Types");
			Output("got %s: %s %s %s", type, uuid, addr, scopes);
			auto dev = std::make_shared<OnvifDevice>("", addr);
			auto ptr = getOnvif(dev->uuid.c_str());
			if (!ptr) {
				dev->GetServices(false, [dev, this](int code, std::string error){
					if (code) return;
					addOnvif(dev);
				});
			}
		}
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
		std::string uuid;
		if (dev.child("UDN")) {
			uuid = dev.child("UDN").child_value();
		}
		else {
			auto pos = usn.find("::");// "::urn:");
			if (-1 != pos && pos > 0)
				uuid = usn.substr(0, pos);
			else
				uuid = usn;
		}

		Device::Ptr device = _devices[uuid];
		if (!device) {
			device = std::make_shared<Device>();
			device->uuid = uuid;
			_devices[uuid] = device;
		}
		time(&device->lastTick);
		device->location = loc;
		device->friendlyName = dev.child("friendlyName").child_value();
		device->modelName = dev.child("modelName").child_value();
		if (auto url = root.child("URLBase")) {
			device->URLHeader = url.child_value();
			int len = device->URLHeader.length();
			if (len && device->URLHeader[len - 1] == '/') {
				device->URLHeader = device->URLHeader.substr(0, len -1);
			}
		}
		device->set_location(loc);

		auto services = device->services_;
		device->services_.clear();
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
			auto serviceType = service.child("serviceType").child_value();
			auto serviceId = service.child("serviceId").child_value();
			auto type = getServiceId(serviceId);
			if (type != USInvalid) {
				type = getServiceType(serviceType);
			}
			if (type == USInvalid) {
				Output("%s skip unknown service %s %s", uuid.c_str(), serviceType, serviceId);
				continue;
			}

			auto controlURL = service.child("controlURL").child_value();
			auto eventSubURL = service.child("eventSubURL").child_value();
			auto SCPDURL = service.child("SCPDURL").child_value();
			auto presentationURL = service.child("presentationURL").child_value();
			auto sm = services[type];
			if (!sm) {
				sm = std::make_shared<ServiceModel>();
				sm->serviceId = serviceId;
				sm->serviceType = serviceType;
			}
			device->services_[type] = sm;
			sm->controlURL = controlURL;
			sm->eventSubURL = eventSubURL;
			sm->presentationURL = presentationURL;
			if (sm->scpdURL != SCPDURL && SCPDURL) {
				std::string url = concatUrl(device->URLHeader, SCPDURL);
				if(sm->desc.parseScpd(url))
					sm->scpdURL = SCPDURL;
			}
		}
		Output("%s", device->description().c_str());
		onChange();
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
