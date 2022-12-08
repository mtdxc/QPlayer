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
		if (!strong_self->services.count(eMedia)){
			strong_self->GetCapabilities();
		}
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
	if (it->second.length()) {
		if (cb) cb(0, it->second);
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
			Output("got Url %s", url);
			if (cb) cb(0, url);
			if (auto strong_ptr = weak_self.lock())
				strong_ptr->profiles[profile] = url;
		}
	});
}

void OnvifDevice::GetDeviceInformation(RpcCB cb)
{
	SoapAction action("GetDeviceInformation");
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp) {
		if (cb) cb(code, "");
		if (code) {
			return;
		}
		auto strong_ptr = weak_self.lock();
		if (!strong_ptr) return;

	});
}
