
<center><h1>基于DLNA实现iOS，Android投屏</h1></center>

# <span id=1>SSDP发现设备</span>

SSDP能够在局域网能简单地发现设备提供的服务。SSDP有两种发现方式：主动通知和搜索响应方式。

## 寻址
UPnP 技术是架构在 IP 网络之上。因此拥有一个网络中唯一的 IP 地址是 UPnP 设备正常工作的基础。UPnP 设备首先查看网络中是否有 DHCP 服务器，如果有，那么使用 DHCP 分配的 IP 即可；如果没有，则需要使用LLA技术来为自己找适合的IP地址。

另外，在 UPnP 运行过程中，UPnP 设备都需要周期性检测网络中是否有 DHCP 服务器存在，一旦发现有 DHCP 服务器，就必须终止使用 LLA 技术获取的 IP 地址，改用 DHCP 分配的 IP 地址。

## 发现
### SSDP
SSDP:Simple Sever Discovery Protocol，简单服务发现协议，此协议为网络客户提供一种无需任何配置、管理和维护网络设备服务的机制。此协议采用基于通知和发现路由的多播发现方式实现。协议客户端在保留的多播地址：239.255.255.250:1900（IPV4）发现服务，（IPv6 是：FF0x::C）同时每个设备服务也在此地址上上监听服务发现请求。如果服务监听到的发现请求与此服务相匹配，此服务会使用单播方式响应。

常见的协议请求消息有两种类型，第一种是服务通知，设备和服务使用此类通知消息声明自己存在；第二种是查询请求，协议客户端用此请求查询某种类型的设备和服务。
iOS中使用GCDAsyncUdpSocket发送和接受SSDP请求、响应及通知，安卓也需要用类此框架来完成

所以我们发现设备也有两种方法

- 主动通知方式：当设备加入到网络中，向网络上所有控制点通知它所提供的服务，通知消息采用多播方式。
- 搜索—响应方式：当一个控制点加入到网络中，在网络搜索它感兴趣的所有设备和服务，搜索消息采用多播方式发送，而设备针对搜索的响应则是使用单播方式发送。

#### SSDP 设备类型及服务类型
| 设备类型 |	表示文字  |
|-|-|
| UPnP_RootDevice | 	upnp:rootdevice |
| UPnP_InternetGatewayDevice1 | 	urn:schemas-upnp-org:device:InternetGatewayDevice:1 |
| UPnP_WANConnectionDevice1 | 	urn:schemas-upnp-org:device:WANConnectionDevice:1 |
| UPnP_WANDevice1 | 	urn:schemas-upnp-org:device:WANConnectionDevice:1 |
| UPnP_WANCommonInterfaceConfig1 | 	urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1 |
| UPnP_WANIPConnection1	 | urn:schemas-upnp-org:device:WANConnectionDevice:1 |
| UPnP_Layer3Forwarding1	 | urn:schemas-upnp-org:service:WANIPConnection:1 |
| UPnP_WANConnectionDevice1	 | urn:schemas-upnp-org:service:Layer3Forwarding:1 |

| 服务类型 |	表示文字 |
|--|--|
| UPnP_MediaServer1     | urn:schemas-upnp-org:device:MediaServer:1 |
| UPnP_MediaRenderer1	| urn:schemas-upnp-org:device:MediaRenderer:1 |
| UPnP_ContentDirectory1	| urn:schemas-upnp-org:service:ContentDirectory:1 |
| UPnP_RenderingControl1	| urn:schemas-upnp-org:service:RenderingControl:1 |
| UPnP_ConnectionManager1	| urn:schemas-upnp-org:service:ConnectionManager:1 |
| UPnP_AVTransport1	| urn:schemas-upnp-org:service:AVTransport:1 |

#### 主动通知方式
当设备添加到网络后，定期向（239.255.255.250:1900）发送SSDP通知消息宣告自己的设备和服务。

宣告消息分为 ssdp:alive(设备可用) 和 ssdp:byebye(设备不可用)

##### ssdp:alive 消息
```
NOTIFY * HTTP/1.1           // 消息头
NT:                         // 在此消息中，NT头必须为服务的服务类型。（如：upnp:rootdevice）
HOST:                       // 设置为协议保留多播地址和端口，必须是：239.255.255.250:1900（IPv4）或FF0x::C(IPv6)
NTS:                        // 表示通知消息的子类型，必须为ssdp:alive
LOCATION:                   // 包含根设备描述URL地址，device的webservice路径（如：http://127.0.0.1:2351/description.xml)
CACHE-CONTROL:              // max-age指定通知消息存活时间，如果超过此时间间隔，控制点可认为设备不存在 （如：max-age=1800）
SERVER:                     // 包含操作系统名，版本，产品名和产品版本信息( 如：Windows NT/5.0, UPnP/1.0)
USN:                        // 表示不同服务的统一服务名，它提供了一种标识出相同类型服务的能力。如：
                            // 根/启动设备 uuid:f7001351-cf4f-4edd-b3df-4b04792d0e8a::upnp:rootdevice
                            // 连接管理器  uuid:f7001351-cf4f-4edd-b3df-4b04792d0e8a::urn:schemas-upnp-org:service:ConnectionManager:1
                            // 内容管理器 uuid:f7001351-cf4f-4edd-b3df-4b04792d0e8a::urn:schemas-upnp-org:service:ContentDirectory:1
```                            
##### ssdp:byebye 消息
当设备即将从网络中退出时，设备需要对每一个未超期的 ssdp:alive 消息多播形式发送 ssdp:byebye 消息，其格式如下：
```
NOTIFY * HTTP/1.1       // 消息头
HOST:                   // 设置为协议保留多播地址和端口，必须是：239.255.255.250:1900（IPv4）或FF0x::C(IPv6
NTS:                    // 表示通知消息的子类型，必须为ssdp:byebye
USN:                    // 同上
```
#### 搜索——响应方式
当控制点，如手机客户端，加入到网络中，可以通过多播搜索消息来寻找网络上感兴趣的设备。我写DLNA模块时候也用主动搜索方式来发现设备。主动搜索可以使用多播方式在整个网络上搜索设备和服务，也可以使用单播方式搜索特定主机上的设备和服务。

##### 多播搜索消息
一般情况我们使用多播搜索消息来搜索所有设备即可。多播搜索消息如下：
```
M-SEARCH * HTTP/1.1             // 请求头 不可改变
MAN: "ssdp:discover"            // 设置协议查询的类型，必须是：ssdp:discover
MX: 5                           // 设置设备响应最长等待时间，设备响应在0和这个值之间随机选择响应延迟的值。这样可以为控制点响应平衡网络负载。
HOST: 239.255.255.250:1900      // 设置为协议保留多播地址和端口，必须是：239.255.255.250:1900（IPv4）或FF0x::C(IPv6)
ST: upnp:rootdevice     // 设置服务查询的目标，它必须是下面的类型：
                        // ssdp:all  搜索所有设备和服务
                        // upnp:rootdevice  仅搜索网络中的根设备
                        // uuid:device-UUID  查询UUID标识的设备
                        // urn:schemas-upnp-org:device:device-Type:version  查询device-Type字段指定的设备类型，device-Type和version由UPNP组织定义。
                        // urn:schemas-upnp-org:service:service-Type:version  查询service-Type字段指定的服务类型，service-Type和version由UPNP组织定义。
```                                
如果需要实现投屏，则设备类型 ST 为 urn:schemas-upnp-org:service:AVTransport:1

##### 多播搜索响应
多播搜索 M-SEARCH 响应与通知消息很类似，只是将NT字段改成ST字段。响应必须以一下格式发送：
```
HTTP/1.1 200 OK             // * 消息头
LOCATION:                   // * 包含根设备描述URL地址，device的webservice路径（如：http://127.0.0.1:2351/description.xml)
CACHE-CONTROL:              // * max-age指定通知消息存活时间，如果超过此时间间隔，控制点可以认为设备不存在 （如：max-age=1800）
SERVER:                     // 包含操作系统名，版本，产品名和产品版本信息( 如：Windows NT/5.0, UPnP/1.0)
EXT:                        // 为了符合HTTP协议要求，并未使用。
BOOTID.UPNP.ORG:            // 可以不存在，初始值为时间戳，每当设备重启并加入到网络时+1，用于判断设备是否重启。也可以用于区分多宿主设备。
CONFIGID.UPNP.ORG:          // 可以不存在，由两部分组成的非负十六进制整数，由两部分组成，第一部分代表跟设备和其上的嵌入式设备，第二部分代表这些设备上的服务。
USN:                        // * 表示不同服务的统一服务名
ST:                         // * 服务的服务类型
DATE:                       // 响应生成时间
```
其中主要关注带有 * 的部分即可。这里还有一个大坑，<b>有些设备返回来的字段名称可能包含有小写，如LOCATION和Location，需要做处理</b>。
此外还需根据LOCATION保存设备的IP和端口地址。

响应例子如下：
```
HTTP/1.1 200 OK
Cache-control: max-age=1800
Usn: uuid:88024158-a0e8-2dd5-ffff-ffffc7831a22::urn:schemas-upnp-org:service:AVTransport:1
Location: http://192.168.1.243:46201/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/desc.xml
Server: Linux/3.10.33 UPnP/1.0 Teleal-Cling/1.0
Date: Tue, 01 Mar 2016 08:47:42 GMT+00:00
Ext:
St: urn:schemas-upnp-org:service:AVTransport:1
```
## 描述
控制点发现设备之后仍然对设备知之甚少，仅能知道UPnP类型，UUID和设备描述URL。为了进一步了解设备和服务，需要获取并解析XML描述文件。
描述文件有两种类型：设备描述文档(DDD)和服务描述文档(SDD)

### 设备描述文档
设备描述文档是对设备的基本信息描述，包括厂商制造商信息、设备信息、设备所包含服务基本信息等。

设备描述采用XML格式，可以通过HTTP GET请求获取。其链接为设备发现消息中的Location。如上述设备的描述文件获取请求为
```
GET http://192.168.1.243:46201/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/desc.xml HTTP/1.1
HOST: 192.168.1.243:46201
```
设备响应如下
```xml
HTTP/1.1 200 OK
Content-Length    : 3612
Content-type      : text/xml
Date              : Tue, 01 Mar 2016 10:00:36 GMT+00:00

<?xml version="1.0" encoding="UTF-8"?>
<root xmlns="urn:schemas-upnp-org:device-1-0" xmlns:qq="http://www.tencent.com">
    <specVersion>
        <major>1</major>
        <minor>0</minor>
    </specVersion>
    <device>
        <deviceType>urn:schemas-upnp-org:device:MediaRenderer:1</deviceType>
        <UDN>uuid:88024158-a0e8-2dd5-ffff-ffffc7831a22</UDN>
        <friendlyName>客厅的小米盒子</friendlyName>
        <qq:X_QPlay_SoftwareCapability>QPlay:1</qq:X_QPlay_SoftwareCapability>
        <manufacturer>Xiaomi</manufacturer>
        <manufacturerURL>http://www.xiaomi.com/</manufacturerURL>
        <modelDescription>Xiaomi MediaRenderer</modelDescription>
        <modelName>Xiaomi MediaRenderer</modelName>
        <modelNumber>1</modelNumber>
        <modelURL>http://www.xiaomi.com/hezi</modelURL>
        <serialNumber>11262/180303452</serialNumber>
        <presentationURL>device_presentation_page.html</presentationURL>
        <UPC>123456789012</UPC>
        <dlna:X_DLNADOC xmlns:dlna="urn:schemas-dlna-org:device-1-0">DMR-1.50</dlna:X_DLNADOC>
        <dlna:X_DLNACAP xmlns:dlna="urn:schemas-dlna-org:device-1-0">,</dlna:X_DLNACAP>
        <iconList>
            <icon>
                <mimetype>image/png</mimetype>
                <width>128</width>
                <height>128</height>
                <depth>8</depth>
                <url>icon/icon128x128.png</url>
            </icon>
        </iconList>
        <serviceList>
            <service>
                <serviceType>urn:schemas-upnp-org:service:AVTransport:1</serviceType>
                <serviceId>urn:upnp-org:serviceId:AVTransport</serviceId>
                <controlURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/AVTransport/action</controlURL>
                <eventSubURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/AVTransport/event</eventSubURL>
                <SCPDURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/AVTransport/desc.xml</SCPDURL>
            </service>
            <service>
                <serviceType>urn:schemas-upnp-org:service:RenderingControl:1</serviceType>
                <serviceId>urn:upnp-org:serviceId:RenderingControl</serviceId>
                <controlURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/RenderingControl/action</controlURL>
                <eventSubURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/RenderingControl/event</eventSubURL>
                <SCPDURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/RenderingControl/desc.xml</SCPDURL>
            </service>
            <service>
                <serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>
                <serviceId>urn:upnp-org:serviceId:ConnectionManager</serviceId>
                <controlURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/ConnectionManager/action</controlURL>
                <eventSubURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/ConnectionManager/event</eventSubURL>
                <SCPDURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/ConnectionManager/desc.xml</SCPDURL>
            </service>
            <service>
                <serviceType>urn:mi-com:service:RController:1</serviceType>
                <serviceId>urn:upnp-org:serviceId:RController</serviceId>
                <controlURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/RController/action</controlURL>
                <eventSubURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/RController/event</eventSubURL>
                <SCPDURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/RController/desc.xml</SCPDURL>
            </service>
        </serviceList>
        <av:X_RController_DeviceInfo xmlns:av="urn:mi-com:av">
            <av:X_RController_Version>1.0</av:X_RController_Version>
            <av:X_RController_ServiceList>
                <av:X_RController_Service>
                    <av:X_RController_ServiceType>controller</av:X_RController_ServiceType>
                    <av:X_RController_ActionList_URL>http://192.168.1.243:6095/</av:X_RController_ActionList_URL>
                </av:X_RController_Service>
                <av:X_RController_Service>
                    <av:X_RController_ServiceType>data</av:X_RController_ServiceType>
                    <av:X_RController_ActionList_URL>http://api.tv.duokanbox.com/bolt/3party/</av:X_RController_ActionList_URL>
                </av:X_RController_Service>
            </av:X_RController_ServiceList>
        </av:X_RController_DeviceInfo>
    </device>
</root>
```
其中响应消息体为XML格式的设备描述内容。信息结构比较明确，就不一一介绍了。解析该XML，保存设备的一些基本信息如 <b>deviceType 、 friendlyName 、 iconList</b> 等。之后我们关注该设备提供的服务列表，投屏最关注的服务为<b> urn:schemas-upnp-org:service:AVTransport:1 </b>:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<root xmlns="urn:schemas-upnp-org:device-1-0" xmlns:qq="http://www.tencent.com">
    <device>
        <serviceList>
            <service>
                <serviceType>urn:schemas-upnp-org:service:AVTransport:1</serviceType>
                <serviceId>urn:upnp-org:serviceId:AVTransport</serviceId>
                <controlURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/AVTransport/action</controlURL>
                <eventSubURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/AVTransport/event</eventSubURL>
                <SCPDURL>/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/AVTransport/desc.xml</SCPDURL>
            </service>
        </serviceList>
    </device>
</root>
```
serviceId : 必有字段。服务表示符，是服务实例的唯一标识。
serviceType : 必有字段。UPnP服务类型。格式定义与deviceType类此。详看文章开头表格。
SCPDURL : 必有字段。Service Control Protocol Description URL，获取设备描述文档URL。
controlURL : 必有字段。向服务发出控制消息的URL，详见 [SOAP控制设备](#2)
eventSubURL : 必有字段。订阅该服务时间的URL，详见 [SOAP控制设备](#2)
如只需要实现简单的投屏，则保存urn:schemas-upnp-org:service:AVTransport:1服务的上述信息即可。如需要进一步了解该服务，则需要获取并解析服务描述文档。

坑点1：有些设备 SCPDURL 、 controlURL 、 eventSubURL 开头包含 / ，有些设备不包含，拼接URL时需要注意。

### 服务描述文档
为了实现简单的投屏和控制（播放、暂停、停止、快进）操作并不需要解析服务描述文件。所有动作均为UPnP规范动作，具体动作请求参见 [SOAP控制设备](#2)

服务描述文档是对服务功能的基本说明，包括服务上的动作及参数，还有状态变量和其数据类型、取值范围等。

和设备描述文档一致，服务描述文档也是采用XML语法，并遵守标准UPnP服务schema文件格式要求。获取上述服务SDD语法如下：
```
GET http://192.168.1.243:46201/dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/AVTransport/desc.xml
HOST: 192.168.1.243:46201
```
设备响应如下：
```xml
HTTP/1.1 200 OK
Content-Length    : 3612
Content-type      : text/xml
Date              : Tue, 01 Mar 2016 10:00:36 GMT+00:00
<!-- 省略了部分动作和状态变量 -->
<?xml version="1.0" encoding="UTF-8"?>
<scpd xmlns="urn:schemas-upnp-org:service-1-0">
    <specVersion>
        <major>1</major>
        <minor>0</minor>
    </specVersion>
    <actionList>
        <action>
            <name>Pause</name>
            <argumentList>
                <argument>
                    <name>InstanceID</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
        <action>
            <name>Play</name>
            <argumentList>
                <argument>
                    <name>InstanceID</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
                </argument>
                <argument>
                    <name>Speed</name>
                    <direction>in</direction>
                    <relatedStateVariable>TransportPlaySpeed</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
        <action>
            <name>Previous</name>
            <argumentList>
                <argument>
                    <name>InstanceID</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
        <action>
            <name>SetAVTransportURI</name>
            <argumentList>
                <argument>
                    <name>InstanceID</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
                </argument>
                <argument>
                    <name>CurrentURI</name>
                    <direction>in</direction>
                    <relatedStateVariable>AVTransportURI</relatedStateVariable>
                </argument>
                <argument>
                    <name>CurrentURIMetaData</name>
                    <direction>in</direction>
                    <relatedStateVariable>AVTransportURIMetaData</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
        ...
    </actionList>
    <serviceStateTable>
        <stateVariable sendEvents="no">
            <name>CurrentTrackURI</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>CurrentMediaDuration</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>AbsoluteCounterPosition</name>
            <dataType>i4</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>RelativeCounterPosition</name>
            <dataType>i4</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_InstanceID</name>
            <dataType>ui4</dataType>
        </stateVariable>
        ...
    </serviceStateTable>
</scpd>
```
- actionList 目前服务上所包含的动作列表。
- serviceStateTable 目前服务上所包含的状态变量。

以Pause动作为例
```xml
<?xml version="1.0" encoding="UTF-8"?>
<scpd xmlns="urn:schemas-upnp-org:service-1-0">
    <actionList>
        <action>
            <!-- 动作名称 -->
            <name>Pause</name>
            <!-- 参数列表 -->
            <argumentList>
                <argument>
                    <!-- 参数名称 -->
                    <name>InstanceID</name>
                    <!-- 输出或输出-->
                    <direction>in</direction>
                    <!-- 声明参数有关的状态变量 -->
                    <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
        ...
    </actionList>
    <serviceStateTable>
        <!-- 状态变量 -->
        <stateVariable>
            <!-- 是否发送事件消息，如为yes则该状态变量发生变化时生成事件消息。 -->
            <stateVariable sendEvents="no">
            <!-- 状态变量名称 -->
            <name>A_ARG_TYPE_InstanceID</name>
            <!-- 状态数据类型 -->
            <dataType>ui4</dataType>
        </stateVariable>
        ...
    </serviceStateTable>
</scpd>
```
为了实现简单的投屏和控制（播放、暂停、停止、快进）操作并不需要解析服务描述文件。所有动作均为UPnP规范动作，具体动作请求参见下节。

# <span id=2>SOAP控制设备</span>

UPnP网络中，控制点和服务之间使用简单对象访问协议（Simple Object Access Protocol，SOAP）

根据[基于DLNA实现iOS，Android投屏：SSDP发现设备](#1) 收到设备描述文档（DDD）和服务描述文档（SDD），通过解析DDD获取 <controlURL> 控制点可以知道该设备上某个服务的控制点地址。再通过解析 DDD 中 <action> 中的 <name> 和 <argumentList> 获取该服务动作的动作名称，参数要求。控制点向 controlURL 发出服务调用信息，携带动作名称和相应参数来调用相应的服务。

## SOAP简单对象访问协议
控制点和服务之间使用简单对象访问协议（Simple Object Access Protocol，SOAP）的格式。SOAP 的底层协议一般也是HTTP。在 UPnP 中，把 SOAP 控制/响应信息分成 3 种： UPnP Action Request、UPnP Action Response-Success 和 UPnP Action Response-Error。SOAP 和 SSDP 不一样，所使用的 HTTP 消息是有 Body 内容，Body 部分可以写想要调用的动作，叫做 Action invocation，可能还要传递参数，如想播放一个网络上的视频，就要把视频的URL传过去；服务收到后要 response ，回答能不能执行调用，如果出错则返回一个错误代码。

### 动作调用（UPnP Action Request）
使用POST方法发送控制消息的格式如下
```xml
POST <control URL> HTTP/1.0
Host: hostname:portNumber
Content-Lenght: byte in body
Content-Type: text/xml; charset="utf-8"
SOAPACTION: "urn:schemas-upnp-org:service:serviceType:v#actionName"

<!--必有字段-->
<?xml version="1.0" encoding="utf-8"?>
<!--SOAP必有字段-->
<s:Envelope s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">
    <s:Body>
        <!--Body内部分根据不同动作不同-->
        <!--动作名称-->
        <u:actionName xmlns:u="urn:schemas-upnp-org:service:serviceType:v">
            <!--输入参数名称和值-->
            <argumentName>in arg values</argumentName>
             <!--若有多个参数则需要提供-->
        </u:actionName>
    </s:Body>
</s:Envelope>
```
- control URL: [SSDP发现设备](#1) 中提到的 设备描述文件 中 urn:upnp-org:serviceId:AVTransport 服务的controlURL
- HOST： 上述服务器的根地址和端口号。
- actionName： 需要调用动作的名称，对应相应服务的 服务描述文件 SCPDURL 中的 action 的 name 字段。
- argumentName： 输入参数名称，对应相应服务的 服务描述文件 SCPDURL 中的 action argument name 字段。
- in arg values： 输入参数值，具体的可以通过 ，可以通过 服务描述文件 SCPDURL action relatedStateVariable 提到的状态变量来得知值的类型。
- urn:schemas-upnp-org:service:serviceType:v：对应该 设备描述文件 相应服务的 serviceType 字段。

### 动作响应（UPnP Action Response-Succes）
收到控制点发来的动作调用请求后，设备上的服务必须执行动作调用，并在 30s 内响应。如果动作执行超过30s，则可先返回一个应答消息，等动作执行完成再利用事件机制返回动作响应。
```xml
HTTP/1.0 200 OK                             // 响应成功响应头
Content-Type: text/xml; charset="utf-8"
Date: Tue, 01 Mar 2016 10:00:36 GMT+00:00
Content-Length: byte in body

<?xml version="1.0" encoding="utf-8" standalone="no"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <!--之前部分为固定字段-->
        <!--之前部分为固定字段-->
        <u:actionNameResponse xmlns:u="urn:schemas-upnp-org:service:serviceType:v">
            <!--输出变量名称和值-->
            <arugumentName>out arg value</arugumentName>
            <!--若有多个输出变量则继续写，没有可以不存在输出变量-->
        </u:actionNameResponse>
    </s:Body>
</s:Envelope>
```
- actionNameResponse： 响应的动作名称
- arugumentName： 当动作带有输出变量时必选，输出变量名称
- out arg values： 输出变量名称值

### 动作错误响应（UPnP Action Response-Succes）
如果处理动作过程中出现错误，则返回一个以下格式的错误响应。
```xml
HTTP/1.0 500 Internal Server Error          // 响应成功响应头
Content-Type: text/xml; charset="utf-8"
Date: Tue, 01 Mar 2016 10:00:36 GMT+00:00
Content-Length: byte in body

<?xml version="1.0" encoding="utf-8" standalone="no"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <u:Fault>
            <!--之前部分为固定字段-->
            <faultcode>s:Client</faultcode>
            <faultstring>UPnPError</faultstring>
            <detail>
                <UPnPError xmlns="urn:schemas-upnp-org:control-1-0">
                    <errorCode>402</errorCode>
                    <errorDescription>Invalid or Missing Args</errorDescription>
                </UPnPError>
            </detail>
        </u:actionNameResponse>
    </s:Body>
</s:Envelope>
```
- faultcode： SOAP规定使用元素，调用动作遇到的错误类型，一般为s:Client。
- faultstring： SOAP规定使用元素，值必须为 UPnPError。
- detail： SOAP规定使用元素，错误的详细描述信息。
- UPnPError： UPnP规定元素。
- errorCode： UPnP规定元素，整数。详见下表。
- errorDescription： UPnP规定元素，简短错误描述。

 errorCode | errorDescription | 描述 
-|-|-|-
401|	Invalid Action |	这个服务中没有该名称的动作
402|	Invalid Args	| 参数数据错误 not enough in args, too many in arg, no in arg by that name, one or more in args 之一
403|	Out of Sycs	| 不同步
501|	Action Failed |	可能在当前服务状态下返回，以避免调用此动作
600 ~ 699|	TBD	| 一般动作错误，由 UPnP 论坛技术委员会定义
700 ~ 799|	TBD	|面向标准动作的特定错误，由 UPnP 论坛工作委员会定义
800 ~ 899|	TBD	| 面向非标准动作的特定错误，由 UPnP 厂商会定义

## 投屏基本命令及其响应
所有命令以发向 [SSDP发现设备](#1) 发现的设备。除了网址以外，其余部分均不需要修改。

所有动作请求使用 POST 请求发送，并且请求Header均如下所示，其中：
- control URL: [SSDP发现设备](#1) 中提到的 [设备描述文件](#设备描述文档) 中 urn:upnp-org:serviceId:AVTransport 服务的 \<controlURL>。
- HOST： 上述服务器的根地址和端口号。
- urn:schemas-upnp-org:service:serviceType:v：对应相应设备的 [设备描述文件](#设备描述文档) 相应服务的 serviceType 字段。
- actionName： 需要调用动作的名称，对应相应服务的 [服务描述文档](#服务描述文档) SCPDURL 中的 action 的 name 字段。
```
POST /dev/88024158-a0e8-2dd5-ffff-ffffc7831a22/svc/upnp-org/AVTransport/action HTTP/1.0
Host: 192.168.1.243:46201
Content-Length: byte in body
Content-Type: text/xml; charset="utf-8"
SOAPACTION: "urn:schemas-upnp-org:service:serviceType:v#actionName"
```
下面请求和响应均忽略Header，参数列表中列出Header的SOAPACTION值

### 设置播放资源URI
#### 动作请求
设置当前播放视频动作统一名称为 SetAVTransportURI 。 需要传递参数有

- InstanceID：设置当前播放时期时为 0 即可。
- CurrentURI： 播放资源URI
- CurrentURIMetaData： 媒体meta数据，可为空
- Header_SOAPACTION： “urn:upnp-org:serviceId:AVTransport#SetAVTransportURI”
有些设备传递播放URI后就能直接播放，有些设备设置URI后需要发送播放命令，可以在接收到 SetAVTransportURIResponse 响应后调用播放动作来解决。
```xml
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<s:Envelope s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">
    <s:Body>
        <u:SetAVTransportURI xmlns:u="urn:schemas-upnp-org:service:AVTransport:1">
            <InstanceID>0</InstanceID>
            <CurrentURI>http://125.39.35.130/mp4files/4100000003406F25/clips.vorwaerts-gmbh.de/big_buck_bunny.mp4</CurrentURI>
            <CurrentURIMetaData />
        </u:SetAVTransportURI>
    </s:Body>
</s:Envelope>
#### 响应
<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">
    <s:Body>
        <u:SetAVTransportURIResponse xmlns:u="urn:schemas-upnp-org:service:AVTransport:1"/>
    </s:Body>
</s:Envelope>
```

### 播放
#### 动作请求
播放视频动作统一名称为Play, 需传递参数有

- InstanceID：设置当前播放时期时为 0 即可。
- Speed：播放速度，默认传 1
- Header_SOAPACTION： “urn:upnp-org:serviceId:AVTransport#Pause”
```xml
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <u:Play xmlns:u="urn:schemas-upnp-org:service:AVTransport:1">
            <InstanceID>0</InstanceID>
            <Speed>1</Speed>
        </u:Play>
    </s:Body>
</s:Envelope>

#### 响应
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <u:PlayResponse xmlns:u="urn:schemas-upnp-org:service:AVTransport:1" />
    </s:Body>
</s:Envelope>
```

### 暂停
#### 动作请求
暂停视频动作统一名称为 Pause, 需传递参数有

- InstanceID：设置当前播放时期时为 0 即可。
- Header_SOAPACTION： “urn:upnp-org:serviceId:AVTransport#Pause”
```xml
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <u:Play xmlns:u="urn:schemas-upnp-org:service:AVTransport:1">
            <InstanceID>0</InstanceID>
            <Speed>1</Speed>
        </u:Play>
    </s:Body>
</s:Envelope>
```
#### 响应
```xml
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <u:PlayResponse xmlns:u="urn:schemas-upnp-org:service:AVTransport:1" />
    </s:Body>
</s:Envelope>
```

### 获取播放进度
#### 动作请求
获取播放进度动作统一名称为 GetPositionInfo, 需传递参数有

- InstanceID：设置当前播放时期时为 0 即可。
- MediaDuration： 可以为空。
- Header_SOAPACTION： “urn:upnp-org:serviceId:AVTransport#MediaDuration”

```xml
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <u:GetPositionInfo xmlns:u="urn:schemas-upnp-org:service:AVTransport:1">
            <InstanceID>0</InstanceID>
            <MediaDuration />
        </u:GetPositionInfo>
    </s:Body>
</s:Envelope>
```
#### 响应
获取播放进度响应中包含了比较多的信息，其中我们主要关心的有一下三个：

- TrackDuration： 目前播放视频时长
- RelTime： 真实播放时长
- AbsTime： 相对播放时长
注：目前为止还没发现 RelTime AbsTime 和不一样的情况，选用 RelTime 就好。

```xml
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <u:GetPositionInfoResponse xmlns:u="urn:schemas-upnp-org:service:AVTransport:1">
            <Track>0</Track>
            <TrackDuration>00:04:32</TrackDuration>
            <TrackMetaData />
            <TrackURI />
            <RelTime>00:00:07</RelTime>
            <AbsTime>00:00:07</AbsTime>
            <RelCount>2147483647</RelCount>
            <AbsCount>2147483647</AbsCount>
        </u:GetPositionInfoResponse>
    </s:Body>
</s:Envelope>
```

### 跳转至特定进度或视频
#### 动作请求
跳转到特定的进度或者特定的视频（多个视频播放情况），需要调用 Seek 动作，传递参数有：

- InstanceID: 一般为 0 。
- Unit：REL_TIME（跳转到某个进度）或 TRACK_NR（跳转到某个视频）。
- Target： 目标值，可以是 00:02:21 格式的进度或者整数的 TRACK_NR。
- Header_SOAPACTION： “urn:upnp-org:serviceId:AVTransport#Seek”

```xml
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <u:Seek xmlns:u="urn:schemas-upnp-org:service:AVTransport:1">
            <InstanceID>0</InstanceID>
            <Unit>REL_TIME</Unit>
            <Target>00:02:21</Target>
        </u:Seek>
    </s:Body>
</s:Envelope>
```
#### 响应
```xml
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <u:SeekResponse xmlns:u="urn:schemas-upnp-org:service:AVTransport:1" />
    </s:Body>
</s:Envelope>
```

## iOS实现
需要用到库
- AEXML - 轻量 XML 库，用于构造和解析XML

### 构造动作XML
首先利用 AEXML 构造动作 XML 部分。由于所有动作结构相似，写了个构造方法
```
private func prepareXMLFileWithCommand(command:AEXMLElement) -> String {
    // 创建 AEXMLDocument 实例
    let soapRequest = AEXMLDocument()
    // 设置XML外层
    let attributes = [
        "xmlns:s" : "http://schemas.xmlsoap.org/soap/envelope/",
        "s:encodingStyle" : "http://schemas.xmlsoap.org/soap/encoding/" ]
    let envelope = soapRequest.addChild(name: "s:Envelope", attributes: attributes)
    let body = envelope.addChild(name: "s:Body")

    // 把 command 添加到 XML 中间
    body.addChild(command)
    return soapRequest.xmlString
}
根据不同动作构造 XML ，比如 传递URI 和 播放动作

/**
投屏

- parameter URI: 视频URL
*/
func SetAVTransportURI(URI:String) {
    let command = AEXMLElement("u:SetAVTransportURI",attributes: ["xmlns:u" : "urn:schemas-upnp-org:service:AVTransport:1"])
    command.addChild(name: "InstanceID", value: "0")
    command.addChild(name: "CurrentURI", value: URI)
    command.addChild(name: "CurrentURIMetaData")
    let xml = self.prepareXMLFileWithCommand(command)

    self.sendRequestWithData(xml,action: "SetAVTransportURI")
}

/**
播放视频
*/
func Play() {
    let command = AEXMLElement("u:Play",attributes: ["xmlns:u" : "urn:schemas-upnp-org:service:AVTransport:1"])
    command.addChild(name: "InstanceID", value: "0")
    command.addChild(name: "Speed", value: "1")
    let xml = self.prepareXMLFileWithCommand(command)

    self.sendRequestWithData(xml,action: "Play")
}
```
### 发送动作请求
```
private func sendRequestWithData(xml:String, action:String) {
    let request = NSMutableURLRequest(URL: NSURL(string: controlURL)!)
    // 使用 POST 请求发送动作
    request.HTTPMethod = "POST"
    request.addValue("text/xml", forHTTPHeaderField: "Content-Type")
    // 添加SOAPAction动作名称
    request.addValue("\(service.serviceId)#\(action)", forHTTPHeaderField: "SOAPAction")
    request.HTTPBody = xml.dataUsingEncoding(NSUTF8StringEncoding)

    let task = NSURLSession.sharedSession().dataTaskWithRequest(request) { data, response, error in
        guard error == nil && data != nil else {
            print("error=\(error)")
            return
        }

        // 检查是否正确响应
        if let httpStatus = response as? NSHTTPURLResponse where httpStatus.statusCode != 200 {
            print("statusCode should be 200, but is \(httpStatus.statusCode)")
            print("response = \(NSString(data: data!, encoding: NSUTF8StringEncoding)))")
        }

        // 解析响应
        self.parseRequestResponseData(data!)

    }
    task.resume()
}
```
### 解析响应
解析请求响应
```
private func parseRequestResponseData(data:NSData) {
    do {
        let xmlDoc = try AEXMLDocument(xmlData: data)

        if let response = xmlDoc.root["s:Body"].first?.children.first {
            switch response.name {
            case "u:SetAVTransportURIResponse":
                print("设置URI成功")
                //获取播放长度
            case "u:GetPositionInfoResponse":
                // 进度需要进一步解析。如realTime = response["RelTime"].value
                print("已获取播放进度")
            case "u:PlayResponse":
                print("已播放")
            case "u:PauseResponse":
                print("已暂停")
            case "u:StopResponse":
                print("已停止")
            default :
                print("未定义响应  - \(xmlDoc.xmlString)")
            }
        } else {
            print("返回不符合规范 - XML:\(xmlDoc.xmlString)")
        }
    }
    catch {
        return
    }
}
```