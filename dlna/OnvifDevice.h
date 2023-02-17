#pragma once
#include <map>
#include <string>
#include <list>
#include <functional>
#include <memory>
#include <vector>

namespace pugi{
	class xml_node;
}

class NetworkInterfaceConnectionSetting
{
public:
	/// @brief Auto negotiation on/off.
	/// Element AutoNegotiation of type xs:boolean.
	bool  AutoNegotiation;	///< Required element.
	/// @brief Speed.
	/// Element Speed of type xs:int.
	int Speed;	///< Required element.
	/// @brief Duplex type, Half or Full.
	/// Element Duplex of type "http://www.onvif.org/ver10/schema":Duplex.
	std::string duplex;	///< Required element. Full or Half
};

class PrefixedIPAddress
{
public:
	/// @brief IPv4 address
	/// Element Address of type "http://www.onvif.org/ver10/schema":IPv4Address.
	std::string Address;	///< Required element.
	/// @brief Prefix/submask length
	/// Element PrefixLength of type xs:int.
	int  PrefixLength;	///< Required element.
	bool Read(pugi::xml_node& node);
	bool Write(pugi::xml_node& node, const std::string& nsp);
};

class IPv4NetworkInterfaceSetConfiguration
{
public:
	/// @brief Indicates whether or not IPv4 is enabled.
	/// Element Enabled of type xs:boolean.
	bool Enabled;	///< Optional element.
	/// @brief List of manually added IPv4 addresses.
	/// Vector of PrefixedIPv4Address* with length 0..unbounded
	std::vector<PrefixedIPAddress> Manual;
	/// @brief Indicates whether or not DHCP is used.
	/// Element DHCP of type xs:boolean.
	bool DHCP;	///< Optional element.
	bool Write(pugi::xml_node& node, const std::string& nsp);
};

enum IPv6DHCPConfiguration
{
	Auto,	///< "Auto"
	Stateful,	///< xs:string value="Stateful"
	Stateless,	///< xs:string value="Stateless"
	Off,	///< xs:string value="Off"
};
const char* IPv6DHCPConfigurationStr(IPv6DHCPConfiguration cfg);
IPv6DHCPConfiguration GetIPv6DHCPConfiguration(const char* cfg);

class IPv6NetworkInterfaceSetConfiguration
{
public:
	/// @brief Indicates whether or not IPv6 is enabled.
	/// Element Enabled of type xs:boolean.
	bool  Enabled ;	///< Optional element.
	/// @brief Indicates whether router advertisment is used.
	/// Element AcceptRouterAdvert of type xs:boolean.
	bool  AcceptRouterAdvert;	///< Optional element.
	/// @brief List of manually added IPv6 addresses.
	/// Vector of PrefixedIPv6Address* with length 0..unbounded
	std::vector<PrefixedIPAddress> Manual;
	/// @brief DHCP configuration.
	/// Element DHCP of type "http://www.onvif.org/ver10/schema":IPv6DHCPConfiguration.
	IPv6DHCPConfiguration      DHCP;	///< Optional element.
	bool Write(pugi::xml_node& node, const std::string& nsp);
};

struct NetworkInterfaceSetConfiguration{
	/// @brief Indicates whether or not an interface is enabled.
	/// Element Enabled of type xs:boolean.
	bool Enabled;	///< Optional element.
	/// @brief Link configuration.
	/// Element Link of type "http://www.onvif.org/ver10/schema":NetworkInterfaceConnectionSetting.
	std::shared_ptr<NetworkInterfaceConnectionSetting>  Link;	///< Optional element.
	/// @brief Maximum transmission unit.
	/// Element MTU of type xs:int.
	int MTU = 0;	///< Optional element.
	/// @brief IPv4 network interface configuration.
	/// Element IPv4 of type "http://www.onvif.org/ver10/schema":IPv4NetworkInterfaceSetConfiguration.
	std::shared_ptr<IPv4NetworkInterfaceSetConfiguration>  IPv4;	///< Optional element.
	/// @brief IPv6 network interface configuration.
	/// Element IPv6 of type "http://www.onvif.org/ver10/schema":IPv6NetworkInterfaceSetConfiguration.
	std::shared_ptr<IPv6NetworkInterfaceSetConfiguration>  IPv6;	///< Optional element.
	/// Element Extension of type "http://www.onvif.org/ver10/schema":NetworkInterfaceSetConfigurationExtension.
	// NetworkInterfaceSetConfigurationExtension*  Extension;	///< Optional element.
	bool Write(pugi::xml_node& node, const std::string& nsp) const;
};

class NetworkInterfaceInfo
{
public:
	/// @brief Network interface name, for example eth0.
	/// Element Name of type xs:string.
	std::string                         Name;	///< Optional element.
	/// @brief Network interface MAC address.
	/// Element HwAddress of type "http://www.onvif.org/ver10/schema":HwAddress.
	std::string                        HwAddress;	///< Required element.
	/// @brief Maximum transmission unit.
	/// Element MTU of type xs:int.
	int  MTU = 0;	///< Optional element.
};

class NetworkInterfaceLink
{
public:
	/// @brief Configured link settings.
	/// Element AdminSettings of type "http://www.onvif.org/ver10/schema":NetworkInterfaceConnectionSetting.
	NetworkInterfaceConnectionSetting  AdminSettings;	///< Required element.
	/// @brief Current active link settings.
	/// Element OperSettings of type "http://www.onvif.org/ver10/schema":NetworkInterfaceConnectionSetting.
	NetworkInterfaceConnectionSetting  OperSettings;	///< Required element.
	/// @brief Integer indicating interface type, for example: 6 is ethernet.
	/// Element InterfaceType of type "http://www.onvif.org/ver10/schema":IANA-IfTypes.
	int InterfaceType;	///< Required element.
	bool Read(pugi::xml_node& node);
	bool Write(pugi::xml_node& node);
};

class IPv4Configuration
{
public:
	/// @brief List of manually added IPv4 addresses.
	/// Vector of PrefixedIPv4Address* with length 0..unbounded
	std::vector<PrefixedIPAddress> Manual;
	/// @brief Link local address.
	/// Element LinkLocal of type "http://www.onvif.org/ver10/schema":PrefixedIPv4Address.
	std::shared_ptr<PrefixedIPAddress> LinkLocal;	///< Optional element.
	/// @brief IPv4 address configured by using DHCP.
	/// Element FromDHCP of type "http://www.onvif.org/ver10/schema":PrefixedIPv4Address.
	std::shared_ptr<PrefixedIPAddress> FromDHCP;	///< Optional element.
	/// @brief Indicates whether or not DHCP is used.
	/// Element DHCP of type xs:boolean.
	bool DHCP;	///< Required element.
	bool Read(pugi::xml_node& node);
	bool Write(pugi::xml_node& node);
};

class IPv6Configuration
{
public:
	/// @brief Indicates whether router advertisment is used.
	/// Element AcceptRouterAdvert of type xs:boolean.
	bool* AcceptRouterAdvert;	///< Optional element.
	/// @brief DHCP configuration.
	/// Element DHCP of type "http://www.onvif.org/ver10/schema":IPv6DHCPConfiguration.
	enum IPv6DHCPConfiguration       DHCP;	///< Required element.
	/// @brief List of manually entered IPv6 addresses.
	/// Vector of PrefixedIPv6Address* with length 0..unbounded
	std::vector<PrefixedIPAddress> Manual;
	/// @brief List of link local IPv6 addresses.
	/// Vector of PrefixedIPv6Address* with length 0..unbounded
	std::vector<PrefixedIPAddress> LinkLocal;
	/// @brief List of IPv6 addresses configured by using DHCP.
	/// Vector of PrefixedIPv6Address* with length 0..unbounded
	std::vector<PrefixedIPAddress> FromDHCP;
	/// @brief List of IPv6 addresses configured by using router advertisment.
	/// Vector of PrefixedIPv6Address* with length 0..unbounded
	std::vector<PrefixedIPAddress> FromRA;
	bool Read(pugi::xml_node& node);
	bool Write(pugi::xml_node& node);
};

class IPv4NetworkInterface
{
public:
	/// @brief Indicates whether or not IPv4 is enabled.
	/// Element Enabled of type xs:boolean.
	bool                            Enabled;	///< Required element.
	/// @brief IPv4 configuration.
	/// Element Config of type "http://www.onvif.org/ver10/schema":IPv4Configuration.
	IPv4Configuration               Config;	///< Required element.
};

class IPv6NetworkInterface
{
public:
	/// @brief Indicates whether or not IPv4 is enabled.
	/// Element Enabled of type xs:boolean.
	bool                            Enabled;	///< Required element.
	/// @brief IPv4 configuration.
	/// Element Config of type "http://www.onvif.org/ver10/schema":IPv4Configuration.
	IPv6Configuration               Config;	///< Required element.
};

struct NetworkInterface {
	/// @brief Unique identifier referencing the physical entity.
	/// Attribute token of type "http://www.onvif.org/ver10/schema":ReferenceToken.
	std::string token; ///< Required element.
	/// Element Enabled of type xs:boolean.
	bool Enabled;	///< Required element.
	/// @brief Network interface information
	/// Element Info of type "http://www.onvif.org/ver10/schema":NetworkInterfaceInfo.
	std::shared_ptr<NetworkInterfaceInfo> Info;	///< Optional element.
	/// @brief Link configuration.
	/// Element Link of type "http://www.onvif.org/ver10/schema":NetworkInterfaceLink.
	std::shared_ptr<NetworkInterfaceLink> Link;	///< Optional element.
	/// @brief IPv4 network interface configuration.
	/// Element IPv4 of type "http://www.onvif.org/ver10/schema":IPv4NetworkInterface.
	std::shared_ptr<IPv4NetworkInterface> IPv4;	///< Optional element.
	/// @brief IPv6 network interface configuration.
	/// Element IPv6 of type "http://www.onvif.org/ver10/schema":IPv6NetworkInterface.
	std::shared_ptr<IPv6NetworkInterface> IPv6;	///< Optional element.
	/// Element Extension of type "http://www.onvif.org/ver10/schema":NetworkInterfaceExtension.
	// NetworkInterfaceExtension*       Extension                      0;	///< Optional element.
	bool Read(pugi::xml_node& node);
	bool Write(pugi::xml_node& node, const std::string& nsp);
};

class IPAddress
{
public:
	enum IPType
	{
		IPv4,	///< xs:string value="IPv4"
		IPv6,	///< xs:string value="IPv6"
	};
	/// @brief Indicates if the address is an IPv4 or IPv6 address.
	/// Element Type of type "http://www.onvif.org/ver10/schema":IPType.
	std::string Type = "IPv4";	///< Required element.
	/// @brief IPv4 address.
	/// Element IPv4Address of type "http://www.onvif.org/ver10/schema":IPv4Address.
	std::string ipAddress;	///< Optional element. IPv4Address / IPv6Address 
	bool Read(pugi::xml_node& node);
	bool Write(pugi::xml_node& node, const std::string& nsp);
};

struct DNSInformation {
	bool FromDHCP;
	// optional
	std::vector<std::string> SearchDomain;
	std::vector<IPAddress> DNSManual;
	std::vector<IPAddress> DNSFromDHCP;
	// Extension optional; [DNSInformationExtension]
	bool Read(pugi::xml_node& node);
	bool Write(pugi::xml_node& node, const std::string& nsp) const;
};

class NetworkGateway
{
public:
	/// @brief IPv4 address string.
	/// Vector of IPv4Address with length 0..unbounded
	std::vector<std::string> IPv4Address;
	/// @brief IPv6 address string.
	/// Vector of IPv6Address with length 0..unbounded
	std::vector<std::string> IPv6Address;
	bool Read(pugi::xml_node& node);
	bool Write(pugi::xml_node& node, const std::string& nsp) const;
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
	void GetHostName(RpcCB cb);
	void SetHostName(const std::string& name, RpcCB cb);

	void GetNetworkInterfaces(std::function<void(int, std::vector<NetworkInterface>& inerfaces)> cb);
	void SetNetworkInterfaces(const std::string& InterfaceToken, const NetworkInterfaceSetConfiguration& in, RpcCB cb);
	void SetNetworkDefaultGateway(const NetworkGateway& gateway, RpcCB cb);
	void GetNetworkDefaultGateway(std::function<void(int, const NetworkGateway&)> cb);

	void SetDNS(const DNSInformation& info, RpcCB cb);
	void GetDNS(std::function<void(int, const DNSInformation&)> cb);

	std::string uuid;
	std::string devUrl;
	std::string name;
	// catalog -> url
	std::map<Catalog, std::string> services;
	// profile -> url
	std::map<std::string, OnvifUrl> profiles;
};

