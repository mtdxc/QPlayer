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

class SoapAction {
	std::string action_;
	std::string nsp_;
	xml_document doc_;
	xml_node ele_;
public:
	SoapAction(const char* name, const char* prefix = "tds", const char* nsp = "http://www.onvif.org/ver10/device/wsdl") : action_(name), nsp_(nsp) {
		auto ele = doc_.append_child("s:Envelope");
		create_attr_node(ele, "xmlns:tt", "http://www.onvif.org/ver10/schema");
		create_attr_node(ele, "xmlns:s", "http://www.w3.org/2003/05/soap-envelope");
		char tmp[32];
		sprintf(tmp, "xmlns:%s", prefix);
		create_attr_node(ele, tmp, nsp);
		sprintf(tmp, "%s:%s", prefix, name);
		ele_ = ele.append_child("s:Body").append_child(tmp);
	}

	void setArgs(const char* name, const char* value);

	std::string getPostXML();

	typedef std::function<void(int, xml_node&)> RpcCB;
	int invoke(const std::string& url, RpcCB cb);
};

void SoapAction::setArgs(const char* name, const char* value)
{
	std::string t = std::string("tds:") + name;
	auto node = ele_.append_child(t.c_str());
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
		std::map<std::string, std::string> mapNS;
		loadDocNsp(doc, mapNS);
		auto soapNs = mapNS["http://www.w3.org/2003/05/soap-envelope"];
		auto body = doc.first_child().child((soapNs + ":Body").c_str());
		auto ns1 = mapNS[ns] + ":" + respTag;
		auto resp = body.child(ns1.c_str());
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
			auto fault = body.child("s:Fault");
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

OnvifDevice::OnvifDevice(const char* id, const char* purl) : uuid(id), devUrl(purl)
{
	auto p = strrchr(id, ':');
	if (p)
		name = p + 1;
	else
		name = id;
}

OnvifDevice::~OnvifDevice()
{
}

void OnvifDevice::GetServices(bool incCapability, RpcCB cb)
{
	SoapAction action("GetServices");
	action.setArgs("IncludeCapability", incCapability?"true":"false");
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(devUrl, [weak_self, cb](int code, xml_node& resp){
		if (cb)
			cb(code, "");
		auto strong_self = weak_self.lock();
		if (!strong_self) return;
		strong_self->services.clear();
		for (auto node : resp)
		{
			auto nsp = node.child_value("tds:Namespace");
			auto url = node.child_value("tds:XAddr");
			Output("%s got service %s, url=%s", strong_self->uuid.c_str(), nsp, url);
			strong_self->services[nsp] = url;
		}
	});
}

bool OnvifDevice::getMediaUrl(std::string& nsp, std::string& url)
{
	nsp = "http://www.onvif.org/ver10/media/wsdl";
	auto it = services.find(nsp);
	if (services.end() == it){
		nsp = "http://www.onvif.org/ver20/media/wsdl";
		it = services.find(nsp);
		if (it == services.end())
			return false;
	}
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
		if (cb)
			cb(code, "");
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
	/*
	<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:trt="http://www.onvif.org/ver10/media/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
  <soap:Body>
    <GetStreamUri xmlns="http://www.onvif.org/ver10/media/wsdl">
      <StreamSetup>
        <!-- Attribute Wild card could not be matched. Generated XML may not be valid. -->
        <Stream xmlns="http://www.onvif.org/ver10/schema">RTP-Unicast</Stream>
        <Transport xmlns="http://www.onvif.org/ver10/schema">
          <Protocol>UDP</Protocol>
        </Transport>
      </StreamSetup>
      <ProfileToken>fixed_prof0</ProfileToken>
    </GetStreamUri>
  </soap:Body>
</soap:Envelope>
	*/
	auto it = profiles.find(profile);
	if (it == profiles.end()) {
		if (cb) cb(-1, "profile no exist");
		return;
	}
	if (it->second.length()) {
		if(cb) cb(0, it->second);
		return;
	}
	std::string nsp, url;
	if (!getMediaUrl(nsp, url))
		return;
	// <soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:trt="http://www.onvif.org/ver10/media/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
	SoapAction action("GetStreamUri", "trt", nsp.c_str());
	action.setArgs("ProfileToken", profile.c_str());
	std::weak_ptr<OnvifDevice> weak_self = shared_from_this();
	action.invoke(url, [weak_self, cb, profile](int code, xml_node& resp) {
		if (code) {
			if (cb) cb(code, "");
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
		auto url = uri.child_value("tt:Uri");
		if (url){
			Output("got Url %s", url);
			if (cb) cb(0, url);
			if (auto strong_ptr = weak_self.lock())
				strong_ptr->profiles[profile] = url;
		}
	});
}
