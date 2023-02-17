#include "OnvifDevice.h"
#include "UpnpServer.h"
#include "httplib.h"
#include "pugixml.hpp"

using namespace pugi;
template<typename T>
bool create_attr_node(xml_node& node, const char_t* pAttrName, const T& attrVal) {
	pugi::xml_attribute attr = node.append_attribute(pAttrName);
	return (attr && attr.set_value(attrVal));
}

bool create_child_node(xml_node& node, const char_t* name, const char* value) {
	auto attr = node.append_child(name);
	return attr && attr.text().set(value);
}

class SoapAction {
	std::string action_;
	std::string nsp_, prefix_;
	xml_document doc_;
	xml_node ele_;
public:
	SoapAction(const char* name, const char* prefix = "tds", const char* nsp = "http://www.onvif.org/ver10/device/wsdl") : action_(name), nsp_(nsp), prefix_(prefix){
		auto ele = doc_.append_child("s:Envelope");
		create_attr_node(ele, "xmlns:tt", "http://www.onvif.org/ver10/schema");
		create_attr_node(ele, "xmlns:s", "http://www.w3.org/2003/05/soap-envelope");
		char tmp[32];
		sprintf(tmp, "xmlns:%s", prefix);
		create_attr_node(ele, tmp, nsp);
		sprintf(tmp, "%s:%s", prefix, name);
		ele_ = ele.append_child("s:Body").append_child(tmp);
	}

	void setArgs(const char* name, const char* value, const char* prefix = nullptr);
	xml_node getReq() { return ele_; }
	std::string getPostXML();

	typedef std::function<void(int, xml_node&)> RpcCB;
	int invoke(const std::string& url, RpcCB cb);
};

void SoapAction::setArgs(const char* name, const char* value, const char* prefix)
{
	char nodeName[128];
	if (!prefix && prefix_.length()) 
		prefix = prefix_.c_str();
	if (prefix && prefix[0])
		sprintf(nodeName, "%s:%s", prefix, name);
	else
		strcpy(nodeName, name);
	auto node = ele_.append_child(nodeName);
	node.text().set(value);
}

std::string SoapAction::getPostXML()
{
	auto eve = doc_.child("s:Envelope");
	//create_attr_node(eve, "xmlns:u", getServiceTypeStr(type_));
	std::ostringstream stm;
	doc_.save(stm);
	return stm.str();
}

int SoapAction::invoke(const std::string& url, RpcCB cb)
{
	static std::atomic<int> action_id(0);
	int id = action_id++;
	std::string action = action_;
	std::string respTag = action_ + "Response";

	httplib::Request req;
	// application/soap+xml; charset=utf-8
	req.set_header("Content-Type", "text/xml");
	req.body = getPostXML();
	req.method = "POST";
	std::string host, ns = nsp_;
	parseUrl(url, host, req.path);
	Output("soap %d> %s", id, req.body.c_str());
	auto task = Upnp::Instance()->getTaskQueue();
	if (!task) return 0;
	task->enqueue([=](){
		httplib::Client http(host);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
		// Authorization: Digest username="admin", realm="Login to 11839e10161ff4ebab8e3260b8779f14", qop="auth", algorithm="MD5", uri="/onvif/device_service", nonce="b252aWYtZGlnZXN0OjQzMjgzNzc2MDMw", nc=00000001, cnonce="2F3F4FD16DDB1CB0558D40A71B48B863", opaque="", response="0159f6acf4719293e8dcd28aaeabc910"
		http.set_digest_auth(Upnp::Instance()->onvif_user, Upnp::Instance()->onvif_pwd);
#else
		http.set_basic_auth(Upnp::Instance()->onvif_user, Upnp::Instance()->onvif_pwd);
#endif
		auto res = http.send(req);
		xml_node args;
		args.set_name("error");
		if (!res) {
			args.set_value("without response");
			cb(-1, args);
			return;
		}
		std::string sbody = res->body;
		Output("soap %d< %d %s", id, res->status, sbody.c_str());
		if (res->status != 200) {
			args.set_value("status code error");
			cb(res->status, args);
			return;
		}
		xml_document doc;
		auto ret = doc.load_string(sbody.c_str());
		if (ret.status) {
			args.set_value("xml parse error");
			//args["detail"] = ret.description();
			cb(-3, args);
			return;
		}
		/*
		<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:SOAP-ENC="http://www.w3.org/2003/05/soap-encoding" xmlns:tds="http://www.onvif.org/ver10/device/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
		<SOAP-ENV:Header />
		<SOAP-ENV:Body>
		<tds:GetServicesResponse>
		*/
		auto body = child_node(doc.first_child(), "Body");
		auto resp = child_node(body, respTag.c_str());
		if (resp) {
			// xml to map
			cb(0, resp);
		}
		else {
			/* <s:Body><s:Fault>
			<faultcode>s:Client</faultcode>
			<faultstring>UPnPError</faultstring>
			<detail>
			<UPnPError xmlns="urn:schemas-upnp-org:control-1-0">
			<errorCode>501</errorCode>
			<errorDescription>Action Failed</errorDescription>
			</UPnPError>
			</detail>
			</s:Fault></s:Body>*/
			auto fault = child_node(body, "Fault");
			cb(-4, fault);
		}
	});
	return id;
}

void loadDocNsp(xml_document &doc, std::map<std::string, std::string> &mapNS)
{
	auto root = doc.first_child();
	for (auto attr : root.attributes()) {
		const char* value = attr.name();
		const char* p = strchr(value, ':');
		if (p)
			value = p + 1;
		mapNS[attr.value()] = value;
	}
}

const static std::string catalog[] = { "All", "Analytics", "Device", "Events", "Imaging", "Media", "PTZ" };
const char* OnvifDevice::CatalogName(Catalog cat) {
	return catalog[cat].c_str();
}

OnvifDevice::Catalog OnvifDevice::CatalogByName(const char* name) {
	int i = 0;
	for (; i < sizeof(catalog) / sizeof(catalog[0]); i++)
	{
		if (!stricmp(catalog[i].c_str(), name))
			return (Catalog)i;
	}
	return eAll;
}

std::map<std::string, OnvifDevice::Catalog> catNspMap;
OnvifDevice::OnvifDevice(const char* id, const char* purl) : uuid(id), devUrl(purl)
{
	static std::once_flag oc;
	std::call_once(oc, [] {
		catNspMap["http://www.onvif.org/ver10/media/wsdl"] = eMedia;
		catNspMap["http://www.onvif.org/ver20/media/wsdl"] = eMedia;
		catNspMap["http://www.onvif.org/ver10/device/wsdl"] = eDevice;
		catNspMap["http://www.onvif.org/ver20/device/wsdl"] = eDevice;
		catNspMap["http://www.onvif.org/ver10/events/wsdl"] = eEvents;
		catNspMap["http://www.onvif.org/ver20/events/wsdl"] = eEvents;
		catNspMap["http://www.onvif.org/ver20/ptz/wsdl"] = ePTZ;
		catNspMap["http://www.onvif.org/ver20/analytics/wsdl"] = eAnalytics;
		catNspMap["http://www.onvif.org/ver20/imaging/wsdl"] = eImage;
	});

	const char* p = nullptr;
	if (uuid.empty()) {
		const char* start = purl;
		p = strstr(start, "://");
		if (p){
			start = p + 3;
		}
		p = strchr(start, '/');
		if (p) {
			name = std::string(start, p);
		}
		else{
			name = start;
		}
		uuid = name;
	}
	else{
		p = strrchr(id, ':');
		if (p)
			name = p + 1;
		else
			name = id;
	}
}

OnvifDevice::~OnvifDevice()
{
}

void OnvifDevice::GetCapabilities(Catalog category, RpcCB cb)
{
	// <GetCapabilities xmlns="http://www.onvif.org/ver10/device/wsdl"><Category>All</Category></GetCapabilities>
	SoapAction action("GetCapabilities");
	action.setArgs("Category", CatalogName(category));
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp){
		if (cb) cb(code, "");
		auto strong_self = weak_self.lock();
		if (!strong_self) return;
		/*
		<tt:Analytics>
		<tt:XAddr>http://192.168.1.74:80/onvif/analytics</tt:XAddr>
		<tt:RuleSupport>true</tt:RuleSupport>
		<tt:AnalyticsModuleSupport>true</tt:AnalyticsModuleSupport>
		</tt:Analytics>
		<tt:Device>
		<tt:XAddr>http://192.168.1.74:80/onvif/device</tt:XAddr>
		*/
		for (auto cap : resp.first_child())
		{
			auto name = cap.name();
			auto addr = child_text(cap, "XAddr");
			if (!name || !addr) continue;
			if (auto p = strchr(name, ':'))
				name = p + 1;
			Catalog cat = CatalogByName(name);
			if (cat){
				Output("cap: %s %s", name, addr);
				strong_self->services[cat] = addr;
			}
		}
	});
}

void OnvifDevice::GetServices(bool incCapability, RpcCB cb)
{
	SoapAction action("GetServices");
	action.setArgs("IncludeCapability", incCapability ? "true" : "false");
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp){
		if (cb) cb(code, "");
		auto strong_self = weak_self.lock();
		if (!strong_self) return;
		strong_self->services.clear();
		for (auto node : resp)
		{
			auto nsp = child_text(node, "Namespace");
			auto url = child_text(node, "XAddr");
			if (catNspMap.count(nsp)){
				Output("%s got service %s, url=%s", strong_self->uuid.c_str(), nsp, url);
				strong_self->services[catNspMap[nsp]] = url;
			}
			else{
				Output("%s got unknown service %s, url=%s", strong_self->uuid.c_str(), nsp, url);
			}
		}
		if (!strong_self->services.count(eMedia)) {
			strong_self->GetCapabilities();
		}
		// add test code for this
		// strong_self->GetDeviceInformation(nullptr);
		// strong_self->GetNetworkInterfaces(nullptr);
		strong_self->GetNetworkDefaultGateway(nullptr);
		// strong_self->GetDNS(nullptr);
	});
}

bool OnvifDevice::getMediaUrl(std::string& nsp, std::string& url)
{
	auto it = services.find(eMedia);
	if (services.end() == it){
		return false;
	}
	nsp = "http://www.onvif.org/ver10/media/wsdl";
	url = it->second;
	return true;
}


void OnvifDevice::GetProfiles(bool useCache, RpcCB cb)
{
	if (useCache && profiles.size()) {
		cb(0, "");
		return;
	}
	std::string nsp, url;
	if (!getMediaUrl(nsp, url))
		return;
	// <soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:trt="http://www.onvif.org/ver10/media/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
	SoapAction action("GetProfiles", "trt", nsp.c_str());
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(url, [weak_self, cb](int code, xml_node& resp){
		if (cb) cb(code, "");
		auto strong_self = weak_self.lock();
		if (!strong_self) return;
		strong_self->profiles.clear();
		for (auto node : resp)
		{
			auto nsp = node.attribute("token").value();
			Output("%s got profile %s, url=%s", strong_self->uuid.c_str(), nsp, node.child_value());
			strong_self->profiles[nsp];
			// GetStreamUri(nsp, nullptr);
		}
	});
}

void OnvifDevice::GetStreamUri(const std::string& profile, RpcCB cb)
{
	auto it = profiles.find(profile);
	if (it == profiles.end()) {
		if (cb) cb(-1, "profile no exist");
		return;
	}
	if (it->second.media.length()) {
		if (cb) cb(0, it->second.media);
		return;
	}
	std::string nsp, url;
	if (!getMediaUrl(nsp, url))
		return;
	/*
<?xml version="1.0" encoding="utf-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:trt="http://www.onvif.org/ver10/media/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
  <soap:Body>
    <GetStreamUri xmlns="http://www.onvif.org/ver10/media/wsdl">
      <StreamSetup>
        <Stream>RTP-Unicast</Stream>
        <Transport>
          <Protocol>UDP</Protocol>
        </Transport>
      </StreamSetup>
      <ProfileToken>MediaProfile00000</ProfileToken>
    </GetStreamUri>
  </soap:Body>
</soap:Envelope>
	*/
	SoapAction action("GetStreamUri", "trt", nsp.c_str());
	auto req = action.getReq();
	create_attr_node(req, "xmlns", nsp.c_str());
	auto stream = req.append_child("StreamSetup");
	stream.append_child("Stream").text().set("RTP-Unicast");
	stream.append_child("Transport").append_child("Protocol").text().set("UDP");
	req.append_child("ProfileToken").text().set(profile.c_str());
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(url, [weak_self, cb, profile](int code, xml_node& resp) {
		if (cb) cb(code, "");
		if (code) {
			return;
		}
		/*
      <trt:MediaUri>
        <tt:Uri>rtsp://192.168.25.67:554/stream1</tt:Uri>
        <tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>
        <tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>
        <tt:Timeout>PT10S</tt:Timeout>
      </trt:MediaUri>
		*/
		auto uri = resp.first_child();
		auto url = child_text(uri, "Uri");
		if (url){
			Output("got MediaUrl %s", url);
			if (auto strong_ptr = weak_self.lock())
				strong_ptr->profiles[profile].media = url;
			if (cb) cb(0, url);
		}
	});
}

void OnvifDevice::GetSnapshotUri(const std::string& profile, RpcCB cb)
{
	auto it = profiles.find(profile);
	if (it == profiles.end()) {
		if (cb) cb(-1, "profile no exist");
		return;
	}
	if (it->second.snap.length()) {
		if (cb) cb(0, it->second.snap);
		return;
	}
	std::string nsp, url;
	if (!getMediaUrl(nsp, url))
		return;
	SoapAction action("GetSnapshotUri", "trt", nsp.c_str());
	action.setArgs("ProfileToken", profile.c_str());
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(url, [weak_self, cb, profile](int code, xml_node& resp) {
		if (cb) cb(code, "");
		if (code) {
			return;
		}
		/*
		<trt:GetSnapshotUriResponse>
		<trt:MediaUri>
			<tt:Uri>http://192.168.24.246:80/cgi-bin/snapshot.cgi</tt:Uri>
			<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>
			<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>
			<tt:Timeout>PT0S</tt:Timeout>
		</trt:MediaUri>
		</trt:GetSnapshotUriResponse>
		*/
		auto uri = resp.first_child();
		auto url = child_text(uri, "Uri");
		if (url){
			Output("got SnapUrl %s", url);
			if (auto strong_ptr = weak_self.lock())
				strong_ptr->profiles[profile].snap = url;
			if (cb) cb(0, url);
		}
	});
}

void OnvifDevice::GetDeviceInformation(std::function<void(int, DeviceInformation)> cb)
{
	SoapAction action("GetDeviceInformation");
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp) {
		DeviceInformation di;
		if (code) {
			if (cb) cb(code, di);
			return;
		}
		auto strong_ptr = weak_self.lock();
		if (!strong_ptr) code = -1;
		di.Model = child_text(resp, "Model");
		di.Manufacturer = child_text(resp, "Manufacturer");
		di.FirmwareVersion = child_text(resp, "FirmwareVersion");
		di.SerialNumber = child_text(resp, "SerialNumber");
		di.HardwareId = child_text(resp, "HardwareId");
		if(cb) cb(code, di);
	});
}

void OnvifDevice::GetHostName(RpcCB cb)
{
	SoapAction action("GetHostname");
	action.invoke(devUrl, [cb](int code, xml_node& resp) {
		if (code) {
			if (cb) cb(code, "");
			return;
		}
		/*
		<tds:GetHostnameResponse>
		<tds:HostnameInformation>
		<tt:FromDHCP>false</tt:FromDHCP>
		<tt:Name>HOSTNAME</tt:Name>
		<tt:Extension />
		</tds:HostnameInformation>
		</tds:GetHostnameResponse>
		*/
		auto host = resp.first_child();
		auto name = child_text(host, "Name");
		auto dhcp = child_text(host, "FromDHCP");
		Output("GotHostName return %s FromDHCP=%s", name, dhcp);
		if (cb) cb(code, name);
	});
}

void OnvifDevice::SetHostName(const std::string& name, RpcCB cb)
{
	SoapAction action("SetHostname");
	action.setArgs("Name", name.c_str());
	action.invoke(devUrl, [cb](int code, xml_node& resp) {
		if (cb) cb(code, "");
	});
}

void OnvifDevice::GetNetworkInterfaces(std::function<void(int, std::vector<NetworkInterface>& inerfaces)> cb)
{
	SoapAction action("GetNetworkInterfaces");
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp) {
    std::vector<NetworkInterface> nis;
		if (cb) cb(code, nis);
		if (code) {
			return;
		}
		auto strong_ptr = weak_self.lock();
		if (!strong_ptr) return;
		for (auto node : resp)
		{
			if (strcasecmp(skipNsp(node.name()), "NetworkInterfaces"))
				continue;
			NetworkInterface ni;
			if (ni.Read(node))
				nis.push_back(ni);
		}
		if (cb) cb(code, nis);
	});
}

#define ARRAY_SIZE(X) sizeof(X)/sizeof(X[0])
static const std::string gSIPv6DHCPConfiguration[] = { "Auto", "Stateful", "Stateless", "Off" };
const char* IPv6DHCPConfigurationStr(IPv6DHCPConfiguration cfg) {
	return gSIPv6DHCPConfiguration[cfg].c_str();
}

IPv6DHCPConfiguration GetIPv6DHCPConfiguration(const char* cfg){
	int i = 0;
	while (i < ARRAY_SIZE(gSIPv6DHCPConfiguration)){
		if (gSIPv6DHCPConfiguration[i] == cfg)
			return (IPv6DHCPConfiguration)i;
		i++;
	}
	return Auto;
}

bool NetworkInterface::Read(pugi::xml_node& node)
{
	/*
	<tds:NetworkInterfaces token="eth0">
	<tt:Enabled>true</tt:Enabled>
	<tt:Info>
	<tt:Name>eth0</tt:Name>
	<tt:HwAddress>F6:70:00:03:C6:13</tt:HwAddress>
	</tt:Info>
	<tt:IPv4>
	<tt:Enabled>true</tt:Enabled>
	<tt:Config>
	...
	</tt:Config>
	</tt:IPv4>
	</tds:NetworkInterfaces>
	*/
	this->token = get_attr_val(node, "token");
	this->Enabled = child_node(node, "Enabled").text().as_bool();
	if (auto info = child_node(node, "Info")) {
		this->Info.reset(new NetworkInterfaceInfo());
		this->Info->Name = child_text(info, "Name");
		this->Info->HwAddress = child_text(info, "HwAddress");
		if (auto mtu = child_node(info, "MTU"))
			this->Info->MTU = mtu.text().as_int();
	}
	if (auto ip = child_node(node, "IPv4")){
		this->IPv4.reset(new IPv4NetworkInterface);
		this->IPv4->Enabled = child_node(ip, "Enabled").text().as_bool();
		if (auto config = child_node(ip, "Config"))
			this->IPv4->Config.Read(config);
	}
	if (auto ip = child_node(node, "IPv6")){
		this->IPv6.reset(new IPv6NetworkInterface);
		this->IPv6->Enabled = child_node(ip, "Enabled").text().as_bool();
		if (auto config = child_node(ip, "Config"))
			this->IPv6->Config.Read(config);
	}
	return true;
}


bool IPv6Configuration::Read(pugi::xml_node& node)
{
	PrefixedIPAddress ip;
	for (auto manual : node.select_nodes("tt:Manual")){
		if (ip.Read(manual.node()))
			this->Manual.push_back(ip);
	}
	for (auto child : node.select_nodes("tt:FromDHCP")){
		if (ip.Read(child.node()))
			this->FromDHCP.push_back(ip);
	}
	for (auto child : node.select_nodes("tt:LinkLocal")){
		if (ip.Read(child.node()))
			this->LinkLocal.push_back(ip);
	}
	for (auto child : node.select_nodes("tt:FromRA")){
		if (ip.Read(child.node()))
			this->FromRA.push_back(ip);
	}
	this->DHCP = GetIPv6DHCPConfiguration(child_text(node, "DHCP"));
	return true;

}

bool IPv4Configuration::Read(pugi::xml_node& node)
{
	/*
	<tt:Manual>
	<tt:Address>192.168.24.246</tt:Address>
	<tt:PrefixLength>22</tt:PrefixLength>
	</tt:Manual>
	<tt:FromDHCP>
	<tt:Address>192.168.24.246</tt:Address>
	<tt:PrefixLength>22</tt:PrefixLength>
	</tt:FromDHCP>
	<tt:DHCP>false</tt:DHCP>
	*/
	PrefixedIPAddress ip;
	for (auto manual : node.select_nodes("tt:Manual")){
		if (ip.Read(manual.node()))
			this->Manual.push_back(ip);
	}
	if (auto n = child_node(node, "FromDHCP")) {
		this->FromDHCP.reset(new PrefixedIPAddress);
		this->FromDHCP->Read(n);
	}
	if (auto n = child_node(node, "LinkLocal")) {
		this->LinkLocal.reset(new PrefixedIPAddress);
		this->LinkLocal->Read(n);
	}
	this->DHCP = child_node(node, "DHCP").text().as_bool();
	return true;
}

bool PrefixedIPAddress::Read(pugi::xml_node& node)
{
	this->Address = child_text(node, "Address");
	this->PrefixLength = child_node(node, "PrefixLength").text().as_int();
	return true;
}

bool PrefixedIPAddress::Write(pugi::xml_node& node, const std::string& nsp)
{
	auto name = nsp + "Address";
	node.append_child(name.c_str()).text().set(this->Address.c_str());
	name = nsp + "PrefixLength";
	node.append_child(name.c_str()).text().set(this->PrefixLength);
	return true;
}

void OnvifDevice::SetNetworkInterfaces(const std::string& InterfaceToken, const NetworkInterfaceSetConfiguration& in, RpcCB cb)
{
	SoapAction action("SetNetworkInterfaces");
	auto req = action.getReq();
	create_attr_node(req, "xmlns::tt", "http://www.onvif.org/ver10/schema");
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
	action.setArgs("InterfaceToken", InterfaceToken.c_str());
	auto node = req.append_child("tds:NetworkInterface");
	in.Write(node, "tt:");
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp) {
		if (cb) cb(code, "");
	});
}

void OnvifDevice::SetNetworkDefaultGateway(const NetworkGateway& gateway, RpcCB cb)
{
	SoapAction action("SetNetworkDefaultGateway");
	gateway.Write(action.getReq(), "tds:");
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp) {
		if (cb) cb(code, "");
	});
}

void OnvifDevice::GetNetworkDefaultGateway(std::function<void(int, const NetworkGateway&)> cb)
{
	SoapAction action("GetNetworkDefaultGateway");
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp) {
		NetworkGateway ng;
		if (!code) {
			ng.Read(resp.first_child());
		}
		if (cb) cb(code, ng);
	});
}

void OnvifDevice::SetDNS(const DNSInformation& info, RpcCB cb)
{
	SoapAction action("SetDNS");
	info.Write(action.getReq(), "tds:");
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp) {
		if (cb) cb(code, "");
	});
}

void OnvifDevice::GetDNS(std::function<void(int, const DNSInformation&)> cb)
{
	SoapAction action("GetDNS");
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp) {
		DNSInformation ng;
		if (!code) {
			ng.Read(resp.first_child());
		}
		if (cb) cb(code, ng);
	});
}

bool NetworkInterfaceSetConfiguration::Write(pugi::xml_node& node, const std::string& nsp) const
{
	std::string name = nsp + "Enabled";
	node.append_child(name.c_str()).text().set(this->Enabled);
	if (MTU > 0) {
		name = nsp + "MTU";
		node.append_child(name.c_str()).text().set(this->MTU);
	}
	if (Link) {
		name = nsp + "Link";
		auto l = node.append_child(name.c_str());
		name = nsp + "AutoNegotiation";
		l.append_child(name.c_str()).text().set(Link->AutoNegotiation);

		name = nsp + "Speed";
		l.append_child(name.c_str()).text().set(Link->Speed);

		name = nsp + "Duplex";
		l.append_child(name.c_str()).text().set(Link->duplex.c_str());
	}

	if (IPv4) {
		name = nsp + "IPv4";
		IPv4->Write(node.append_child(name.c_str()), nsp);
	}
	if (IPv6) {
		name = nsp + "IPv6";
		IPv6->Write(node.append_child(name.c_str()), nsp);
	}
	return true;
}

bool IPv4NetworkInterfaceSetConfiguration::Write(pugi::xml_node& node, const std::string& nsp)
{
	std::string name = nsp + "Enabled";
	node.append_child(name.c_str()).text().set(this->Enabled);
	name = nsp + "DHCP";
	node.append_child(name.c_str()).text().set(this->DHCP);
	for (auto m : this->Manual)
	{
		name = nsp + "Manual";
		m.Write(node.append_child(name.c_str()), nsp);
	}
	return true;
}

bool IPv6NetworkInterfaceSetConfiguration::Write(pugi::xml_node& node, const std::string& nsp)
{
	std::string name = nsp + "Enabled";
	node.append_child(name.c_str()).text().set(this->Enabled);

	name = nsp + "DHCP";
	node.append_child(name.c_str()).text().set(IPv6DHCPConfigurationStr(this->DHCP));

	for (auto m : this->Manual)
	{
		name = nsp + "Manual";
		m.Write(node.append_child(name.c_str()), nsp);
	}

	name = nsp + "AcceptRouterAdvert";
	node.append_child(name.c_str()).text().set(this->AcceptRouterAdvert);
	return true;
}

bool IPAddress::Read(pugi::xml_node& node)
{
	this->Type = child_text(node, "Type");
	if (Type == "IPv4") {
		ipAddress = child_text(node, "IPv4Address");
	}
	else{
		ipAddress = child_text(node, "IPv6Address");
	}
	return true;
}

bool IPAddress::Write(pugi::xml_node& node, const std::string& nsp)
{
	std::string name = nsp + "Type";
	node.append_child(name.c_str()).text().set(Type.c_str());
	name = nsp + Type + "Address";
	node.append_child(name.c_str()).text().set(ipAddress.c_str());
	return true;
}

bool DNSInformation::Read(pugi::xml_node& node)
{
	FromDHCP = child_node(node, "FromDHCP").text().as_bool();
	IPAddress addr;
	for (auto child : node)
	{
		std::string name = skipNsp(child.name());
		if (name == "SearchDomain") {
			this->SearchDomain.push_back(child.text().as_string());
		}
		else if (name == "DNSManual") {
			if (addr.Read(child))
				DNSManual.push_back(addr);
		}
		else if (name == "DNSFromDHCP") {
			if (addr.Read(child))
				DNSFromDHCP.push_back(addr);
		}
	}
	return true;
}

bool DNSInformation::Write(pugi::xml_node& node, const std::string& nsp) const
{
	auto name = nsp + "FromDHCP";
	node.append_child(name.c_str()).text().set(this->FromDHCP);
	for (auto sd : SearchDomain)
	{
		name = nsp + "SearchDomain";
		node.append_child(name.c_str()).text().set(sd.c_str());
	}
	for (auto sd : DNSManual)
	{
		name = nsp + "DNSManual";
		sd.Write(node.append_child(name.c_str()), nsp);
	}
	for (auto sd : DNSFromDHCP)
	{
		name = nsp + "DNSFromDHCP";
		sd.Write(node.append_child(name.c_str()), nsp);
	}
	return true;
}

bool NetworkGateway::Read(pugi::xml_node& node)
{
	for (auto child : node)
	{
		std::string name = skipNsp(child.name());
		if (name == "IPv4Address") {
			IPv4Address.push_back(child.text().as_string());
		}
		else if (name == "IPv6Address"){
			IPv4Address.push_back(child.text().as_string());
		}
	}
	return true;
}

bool NetworkGateway::Write(pugi::xml_node& node, const std::string& nsp) const
{
	auto name = nsp + "IPv6Address";
	for (auto sd : IPv6Address)
	{
		node.append_child(name.c_str()).text().set(sd.c_str());
	}
	name = nsp + "IPv4Address";
	for (auto sd : IPv4Address)
	{
		node.append_child(name.c_str()).text().set(sd.c_str());
	}
	return true;
}
