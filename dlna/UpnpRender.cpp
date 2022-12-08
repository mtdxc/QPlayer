#include "UpnpRender.h"
#include <sstream>
#include <atomic>
#include "hstring.h"
#include "httplib.h"
#include "pugixml.hpp"

using namespace pugi;
template<typename T>
bool create_attr_node(xml_node& node, const char_t* pAttrName, const T& attrVal) {
	pugi::xml_attribute attr = node.append_attribute(pAttrName);
	return (attr && attr.set_value(attrVal));
}
const char* unitREL_TIME = "REL_TIME";
const char* unitTRACK_NR = "TRACK_NR";

typedef std::map<std::string, std::string> ArgMap;
class UPnPAction {
	UpnpServiceType type_;
	std::string action_;
	xml_document doc_;
	xml_node ele_;
public:
	UPnPAction(const char* name) : action_(name) {
		type_ = USAVTransport;
		std::string t = "u:" + action_;
		auto ele = doc_.append_child("s:Envelope");
		create_attr_node(ele, "s:encodingStyle", "http://schemas.xmlsoap.org/soap/encoding/");
		create_attr_node(ele, "xmlns:s", "http://schemas.xmlsoap.org/soap/envelope/");
		ele_ = ele.append_child("s:Body").append_child(t.c_str());
	}

	void setArgs(const char* name, const char* value);

	void setServiceType(UpnpServiceType t) {
		type_ = t;
	}
	UpnpServiceType getServiceType() const { return type_; }

	std::string getSOAPAction() const;
	std::string getPostXML();

	typedef std::function<void(int, ArgMap&)> RpcCB;
	int invoke(Device::Ptr dev, RpcCB cb);
};

void UPnPAction::setArgs(const char* name, const char* value)
{
	auto node = ele_.append_child(name);
	node.text().set(value);
}

std::string UPnPAction::getSOAPAction() const
{
	char buff[256] = { 0 };
	sprintf(buff, "\"%s#%s\"", getServiceTypeStr(type_), action_.c_str());
	return buff;
}

std::string UPnPAction::getPostXML()
{
	auto eve = doc_.child("s:Envelope");
	create_attr_node(eve, "xmlns:u", getServiceTypeStr(type_));
	std::ostringstream stm;
	doc_.save(stm);
	return stm.str();
}

int UPnPAction::invoke(Device::Ptr dev, RpcCB cb)
{
	static std::atomic<int> action_id(0);
	int id = action_id++;
	std::string action = action_;
	std::string respTag = "u:" + action_ + "Response";
	auto cb1 = [id, action, dev](int code, std::map<std::string, std::string>& args) {
		if (code) {
			Output("soap %d %s resp error %d %s %s", id, action.c_str(),
				code, args["error"].c_str(), args["detail"].c_str());
			dev->lastTick -= DEVICE_TIMEOUT / 3;
		}
		else
			time(&dev->lastTick);
		for (auto listener : Upnp::Instance()->getListeners())
			listener->upnpActionResponse(id, code, args);
	};
	if (!cb) {
		cb = cb1;
	}
	else {
		auto cb2 = cb;
		cb = [cb1, cb2](int code, std::map<std::string, std::string>& args) {
			cb1(code, args);
			cb2(code, args);
		};
	}
	httplib::Request req;
	req.set_header("SOAPAction", getSOAPAction());
	req.set_header("Content-Type", "text/xml");
	req.body = getPostXML();
	req.method = "POST";
	std::string host;
	parseUrl(dev->getControlUrl(type_), host, req.path);
	//Output("soap %d> %s", id, req.body.c_str());
	auto task = Upnp::Instance()->getTaskQueue();
	if(!task) return 0;
	task->enqueue([=](){
		httplib::Client http(host);
		auto res = http.send(req);
		ArgMap args;
		if (!res) {
			args["error"] = "without response";
			cb(-1, args);
			return;
		}
		std::string sbody = res->body;
		// Output("soap %d< %d %s", id, res->status, sbody.c_str());
		if (res->status != 200) {
			args["error"] = "status code error";
			if (sbody.length())
				args["detail"] = sbody;
			cb(res->status, args);
			return;
		}
		xml_document doc;
		auto ret = doc.load_string(sbody.c_str());
		if (ret.status) {
			args["error"] = "xml parse error";
			args["detail"] = ret.description();
			cb(-3, args);
			return;
		}
		/*
		<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body>
		<u:SetAVTransportURIResponse xmlns:u="urn:schemas-upnp-org:service:AVTransport:1"></u:SetAVTransportURIResponse>
		</s:Body></s:Envelope>
		*/
		auto body = child_node(child_node(doc, "Envelope"), "Body");
		if (!body) {
			//LOG(INFO) << "soapResp missing body tag";
			args["error"] = "missing body tag";
			cb(-2, args);
		}
		else {
			auto resp = body.child(respTag.c_str());
			if (resp) {
				// xml to map
				for (auto child : resp.children()) {
					args[child.name()] = child.text().as_string("");
				}
				cb(0, args);
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
				if (fault) {
					args["error"] = fault.child("faultstring").text().as_string();
					std::string code = fault.child("faultcode").text().as_string();
					std::ostringstream stm;
					fault.child("detail").first_child().print(stm, "");
					args["detail"] = stm.str();
				}
				else {
					args["error"] = "missing fault tag";
				}
				cb(-4, args);
			}
		}
	});
	return id;
}

UpnpRender::UpnpRender(Device::Ptr dev) :model_(dev)
{
	if (auto srv = dev->getService(USAVTransport)) {
		support_speed_ = srv->desc.findActionArg("Play", "Speed");
		Output("support_speed=%d", support_speed_);
	}
}

UpnpRender::~UpnpRender()
{
	Output("%s ~UpnpRender", model_->uuid.c_str());
	if (url_.length())
		stop();
	auto sids = sid_map_;
	for (auto it : sids)
		unsubscribe(it.first);
}

void testNotify(const std::string& sid)
{
	httplib::Request req;
	req.method = "NOTIFY";
	req.path = "/";
	req.set_header("Content-Type", "text/xml");
	req.set_header("NT", "upnp:event");
	req.set_header("NTS", "upnp:propchange");
	req.set_header("SID", sid);
	req.set_header("SEQ", "1");
	req.body = "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\"> \
					<e:property><variableName>new value</variableName><e:property> \
				</e:propertyset>";
	if (auto task = Upnp::Instance()->getTaskQueue())
	task->enqueue([=](){
		httplib::Client http(Upnp::Instance()->getUrlPrefix());
		auto res = http.send(req);
		if (!res){
			Output("notify error %s", to_string(res.error()).c_str());
		}
		else if (res->status != 200) {
			Output("notify error %d, %s", res->status, res->body.c_str());
		}
		else{
			Output("notify ok %s", res->body.c_str());
		}
	});
}

void UpnpRender::subscribe(int type, int sec)
{
	auto sevice = model_->getService((UpnpServiceType)type);
	if (sevice->eventSubURL.empty())
		return;

	/*
	SUBSCRIBE publisher_path HTTP/1.1
	HOST: publisher_host:publisher_port
	CALLBACK: <delivery URL>
	NT: upnp:event
	TIMEOUT: second-[requested subscription duration]
	*/
	httplib::Request req;
	req.method = "SUBSCRIBE";
	auto it = sid_map_.find(type);
	if (it==sid_map_.end()) { // È«ÐÂ¶©ÔÄ
		req.set_header("NT", "upnp:event");
		req.set_header("CALLBACK", std::string("<") + Upnp::Instance()->getUrlPrefix() + "/>");
	}
	else{ // Ðø¶©
		req.set_header("SID", it->second);
	}
	char timeout[64];
	if (sec > 0) {
		sprintf(timeout, "Second-%d", sec);
	}
	else{
		strcpy(timeout, "Second-infinite");
	}
	req.set_header("TIMEOUT", timeout);
	req.path = sevice->eventSubURL;
	if (req.path.length() && req.path[0] != '/')
		req.path = "/" + req.path;
	auto host = model_->URLHeader;
	auto task = Upnp::Instance()->getTaskQueue();
	std::weak_ptr<UpnpRender> weak_ptr = shared_from_this();
	task->enqueue([=](){
		httplib::Client http(host);
		auto res = http.send(req);
		if (!res){
			Output("subscribe error %s", to_string(res.error()).c_str());
			return;
		}
		if (res->status != 200) {
			Output("subscribe error %d, %s", res->status, res->body.c_str());
			return;
		}
		/*
		HTTP/1.1 200 OK
		DATE£ºwhen response was generated
		SERVER: OS/version UPnP/1.0 product/version
		SID: uuid:subscription-UUID
		TIMEOUT: second-[actual subscription duration]
		*/
		std::string sid = res->get_header_value("SID");
		Output("subscribe return %s, timeout=%s", sid.c_str(), res->get_header_value("TIMEOUT").c_str());
		auto strong_ptr = weak_ptr.lock();
		if (!strong_ptr) return;
		auto oldsid = strong_ptr->sid_map_[type];
		if (oldsid != sid) {
			if (oldsid.length())
				Upnp::Instance()->delSidListener(oldsid);
			strong_ptr->sid_map_[type] = sid;
			Upnp::Instance()->addSidListener(sid, strong_ptr.get());
		}
		// testNotify(sid);
	});
}

void UpnpRender::unsubscribe(int type)
{
	auto it = sid_map_.find(type);
	if (it == sid_map_.end())
		return;
	std::string sid = it->second;
	sid_map_.erase(it);
	Upnp::Instance()->delSidListener(sid);

	auto sevice = model_->getService((UpnpServiceType)type);
	if (sevice->eventSubURL.empty()){
		return;
	}
	auto host = model_->URLHeader;
	/*
	UNSUBSCRIBE publisher_path HTTP/1.1
	HOST:  publisher_host:publisher_port
	SID: uuid: subscription UUID
	*/
	httplib::Request req;
	req.method = "UNSUBSCRIBE";
	req.path = sevice->eventSubURL;
	if (req.path.length() && req.path[0] != '/')
		req.path = "/" + req.path;
	req.set_header("SID", sid);
	auto task = Upnp::Instance()->getTaskQueue();
	task->enqueue([=](){
		httplib::Client http(host);
		auto res = http.send(req);
		if (!res){
			Output("unsubscribe error %s", to_string(res.error()).c_str());
		}
		else if (res->status != 200) {
			Output("unsubscribe error %d, %s", res->status, res->body.c_str());
		}
		else{
			Output("unsubscribe %s ok", sid.c_str());
		}
	});
}

void UpnpRender::onPropChange(const std::string& name, const std::string& value)
{
	Output("%s onPropChange %s=%s", model_->uuid.c_str(), name.c_str(), value.c_str());
	if (name == "CurrentMediaDuration") {
		duration_ = strToDuraton(value.c_str());
	}
	for (auto l : Upnp::Instance()->getListeners()) {
		l->upnpPropChanged(model_->uuid.c_str(), name.c_str(), value.c_str());
	}
}

void UpnpRender::onSidMsg(const std::string& sid, const std::string& body)
{
	// Output("%s onSidMsg %s", model_->uuid.c_str(), body.c_str());
	xml_document doc;
	auto ret = doc.load_string(body.c_str());
	if (ret.status) {
		Output("onSidMsg xml parse %s error %s", body.c_str(), ret.description());
		return;
	}
	/*
	<e:propertyset xmlns:e="urn:schemas-upnp-org:event-1-0"><e:property>
	<LastChange>&lt;Event xmlns = &quot;urn:schemas-upnp-org:metadata-1-0/AVT/&quot;&gt;&lt;/Event&gt;</LastChange>
	</e:property></e:propertyset>
	*/
	for (xml_node prop = doc.first_child(); prop; prop = prop.next_sibling()) {
		// property
		for (xml_node change = prop.first_child(); change; change = change.next_sibling()) {
			for (auto child : change.children()) {
				std::string name = child.name();
				std::string value = child.text().as_string();
				if (name == "LastChange") {
					if (value.empty()) return;
					/*
					<Event xmlns = "urn:schemas-upnp-org:metadata-1-0/AVT/"><InstanceID val="0">
					<TransportState val="PLAYING"/><TransportStatus val="OK"/>
					</InstanceID></Event>
					*/
					xml_document event;
					auto ret = event.load_string(value.c_str());
					if (ret.status) {
						Output("onSidMsg xml parse %s error %s", value.c_str(), ret.description());
						continue;
					}
					for (auto inst : event.first_child().first_child()) {
						std::string name = inst.name();
						std::string value = inst.attribute("val").as_string();
						onPropChange(name, value);
					}
				}
				else{
					onPropChange(name, value);
				}
			}
		}
	}
}

int UpnpRender::setAVTransportURL(const char* urlStr, RpcCB cb)
{
	subscribe(USAVTransport, -1);
	std::string url = urlStr;
	Output("%s setAVTransportURL %s", model_->uuid.c_str(), urlStr);
	UPnPAction action("SetAVTransportURI");
	action.setArgs("InstanceID", "0");
	action.setArgs("CurrentURI", urlStr);
	action.setArgs("CurrentURIMetaData", "");
	std::weak_ptr<UpnpRender> weak_ptr = shared_from_this();
	return action.invoke(model_, [cb, weak_ptr, url](int code, ArgMap& args) {
		auto strong_ptr = weak_ptr.lock();
		if (!strong_ptr) return;
		if (code) { // return error
			if (cb) cb(code, args["error"]);
			return;
		}
		strong_ptr->url_ = url;
		strong_ptr->getTransportInfo([weak_ptr, cb](int code, TransportInfo ti) {
			if (code) {
				if (cb) cb(code, "getTransportInfo error");
				return;
			}
			auto strong_ptr = weak_ptr.lock();
			if (!strong_ptr) return;
			Output("state=%s, status=%s, speed=%f\n", ti.state, ti.status, ti.speed);
			strong_ptr->speed_ = ti.speed;
			std::string state = ti.state;
			if (state != "PLAYING" && state != "TRANSITIONING")
				strong_ptr->play(ti.speed, cb);
			else if (cb)
				cb(0, "ok");
		});
	});
}

int UpnpRender::play(float speed, RpcCB cb)
{
	Output("%s play %f", model_->uuid.c_str(), speed);
	UPnPAction action("Play");
	action.setArgs("InstanceID", "0");
	if (support_speed_) {
		char buf[32];
		sprintf(buf, "%f", speed);
		action.setArgs("Speed", buf);
	}
	std::weak_ptr<UpnpRender> weak_ptr = shared_from_this();
	return action.invoke(model_, [=](int code, ArgMap& args) {
		auto strong_ptr = weak_ptr.lock();
		if (!strong_ptr) return;
		if (code) {
			if (strong_ptr->support_speed_ && -1 != args["error"].find("Speed not support")) {
				Output("reset support_speed and try");
				strong_ptr->support_speed_ = false;
				strong_ptr->play(1.0f, cb);
				return;
			}
		}
		else
			strong_ptr->speed_ = speed;
		if (cb) cb(code, args["error"]);
	});
}

int UpnpRender::pause(RpcCB cb)
{
	Output("%s pause", model_->uuid.c_str());
	UPnPAction action("Pause");
	action.setArgs("InstanceID", "0");
	return action.invoke(model_, [cb](int code, ArgMap& args) {
		if (cb) cb(code, args["error"]);
	});
}

int UpnpRender::stop(RpcCB cb)
{
	Output("%s stop", model_->uuid.c_str());
	UPnPAction action("Stop");
	action.setArgs("InstanceID", "0");
	url_.clear();
	duration_ = 0;
	return action.invoke(model_, [cb](int code, ArgMap& args) {
		if (cb) cb(code, args["error"]);
	});
}

int UpnpRender::seek(float relTime, RpcCB cb)
{
	char szTime[64];
	int sec = (int)relTime % 3600;
	sprintf(szTime, "%02d:%02d:%02d", (int)relTime / 3600, sec / 60, sec % 60);
	return seekToTarget(szTime, unitREL_TIME, cb);
}

int UpnpRender::seekToTarget(const char* target, const char* unit, RpcCB cb)
{
	Output("%s seekToTarget %s %s", model_->uuid.c_str(), unit, target);
	UPnPAction action("Seek");
	action.setArgs("InstanceID", "0");
	action.setArgs("Unit", unit);
	action.setArgs("Target", target);
	return action.invoke(model_, [cb](int code, ArgMap& args) {
		if (cb) cb(code, args["error"]);
	});
}

float strToDuraton(const char* str) {
	if (!str || !str[0]) return 0;
	auto list = hv::split(str, ':');
	if (list.size() < 3)
		return -1.0f;
	float ret = 0;
	for (int i = 0; i< list.size(); i++)
	{
		int val = std::stoi(list[i]);
		switch (i)
		{
		case 0:
			ret += val * 3600;
			break;
		case 1:
			ret += val * 60;
			break;
		case 2:
			ret += val;
			break;
		case 3:
			ret += val * 0.1;
		default:
			break;
		}
	}
	return ret;
}

int UpnpRender::getPositionInfo(std::function<void(int, AVPositionInfo)> cb)
{
	UPnPAction action("GetPositionInfo");
	action.setArgs("InstanceID", "0");
	return action.invoke(model_, [cb](int code, ArgMap& args) {
		if (!cb) return;
		AVPositionInfo pos;
		if (code == 0) {
			pos.trackDuration = strToDuraton(args["TrackDuration"].c_str());
			pos.relTime = strToDuraton(args["RelTime"].c_str());
			pos.absTime = strToDuraton(args["AbsTime"].c_str());
		}
		cb(code, pos);
	});
}

int UpnpRender::getTransportInfo(std::function<void(int, TransportInfo)> cb)
{
	UPnPAction action("GetTransportInfo");
	action.setArgs("InstanceID", "0");
	return action.invoke(model_, [cb](int code, ArgMap& args) {
		if (!cb) return;
		TransportInfo ti;
		if (code == 0) {
			ti.setState(args["CurrentTransportState"].c_str());
			ti.setStatus(args["CurrentTransportStatus"].c_str());
			ti.speed = std::stof(args["CurrentSpeed"]);
		}
		cb(code, ti);
	});
}

int UpnpRender::getVolume(std::function<void(int, int)> cb)
{
	// subscribe(USRenderingControl);
	UPnPAction action("GetVolume");
	action.setServiceType(USRenderingControl);
	action.setArgs("InstanceID", "0");
	action.setArgs("Channel", "Master");
	return action.invoke(model_, [cb](int code, ArgMap& args) {
		if (!cb) return;
		int vol = -1;
		auto val = args["CurrentVolume"];
		if(val.length())
			vol = std::stoi(val);
		cb(code, vol);
	});
}

int UpnpRender::setVolume(int value, RpcCB cb)
{
	char buff[20];
	sprintf(buff, "%d", value);
	return setVolumeWith(buff, cb);
}

int UpnpRender::setVolumeWith(const char* value, RpcCB cb)
{
	Output("%s setVolumeWith %s", model_->uuid.c_str(), value);
	UPnPAction action("SetVolume");
	action.setServiceType(USRenderingControl);
	action.setArgs("InstanceID", "0");
	action.setArgs("Channel", "Master");
	action.setArgs("DesiredVolume", value);
	return action.invoke(model_, [cb](int code, ArgMap& args) {
		if (cb) cb(code, args["error"]);
	});
}

TransportInfo::TransportInfo()
{
	state[0] = status[0] = 0; speed = 1.0f;
}

void TransportInfo::setState(const char* v)
{
	if (v)
		strncpy(state, v, sizeof(state));
}

void TransportInfo::setStatus(const char* v)
{
	if (v)
		strncpy(status, v, sizeof(status));
}
