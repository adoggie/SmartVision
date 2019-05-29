

# 新可视对讲系统_接口设计_物业服务器接口


Revision:

    1. 2019.1.31 scott created
	
	2019.3.13 v0.2 scott 
	
	2019.3.14 v0.3 scott 
	1. 比较大的调整，增加事件上报和状态上报，去除报警等接口
	
	2019.3.18 v0.4 scott 
	1. initdata 查询返回室外机列表、岗亭机列表
	2. 增加  2.8 发送开门信息
	         2.7 留影留相功能（app上传呼叫视频)
	         2.6 二维码开门转发绿城家
	         
	2019.3.23 0.5  scott 
	1. 格式化文字
	2. initdata 接口，增加室内机处理返回物业中心推流服务器字段
	    - stream_server_url	
	
	2019.4.12 0.6  scott 
	1. 增加  2.9 连接状态监测 ping
	
## 名词定义


## 说明
新版本系统接口设计基于旧版可视对讲系统的基础架构而进行优化和完善。

新系统特点：

1. 室外单元设备从linux主机改为Android主机，门禁交互按键功能由Android的软件实现。Android程序完成门禁消息的响应和控制，并实现单元机与室内机、单元机与中心和单元机与物业后端App的音视频对讲功能以及蓝牙、Qr码、人脸识别开门的功能。 


2. 室内实现主屏分离，由主控设备(SmartBox)和内屏设备（Screen）组成。 Screen设备网络接口和主控设备的内网接口统一接入到家庭网络端口(wifi)，在视频对话时，Screen设备可以发起对室外单元机、物业中心、对点室内机的音视频对讲，并同时能响应来自室外的音视频对讲呼入。

3. Smartbox与Screen：Screen设备通过SmartBox的认证授权之后方可接入到小区网络。Screen发起的查询请求，除了SmartBox自身处理外，其他的被转发到物业服务器处理。

4. 对讲与推流： 新系统与旧系统的推流方式不同，新系统中的推流服务器SS由对讲发起方建立，被叫方与主叫方均将自己的音视频流推送到SS进行推拉交换。


## 错误码


    #define Error_NoError 0
    #define Error_UnknownError 1	// 未定义
    #define Error_SystemFault 1001	// 系统错误
    #define Error_TokenInvalid 1002	// 令牌错误
    #define Error_AccessDenied 1003	// 访问受限
    #define Error_PermissionDenied 1004	// 权限受限
    #define Error_ParameterInvalid 1005	// 参数无效
    #define Error_PasswordError 1006	// 密码错误
    #define Error_UserNotExist 1007		// 用户不存在
    #define Error_ObjectHasExist 1008	// 对象已存在
    #define Error_ObjectNotExist 1009	// 对象不存在
    #define Error_ResExpired 1010		// 资源过期
    #define Error_ReachLimit 1011	 	// 达到上限


### 2.1 初始化环境信息
#### 名称:  
initData()

#### 描述
小区内所有设备上线时查询运行配置信息。服务器会根据请求者的ip自动识别设备类型，并返回不同的响应消息。
 
#### Request
<pre>
URL: /propserver/api/initdata
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
   - type			1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App; 5:物业服务器
</pre>

#### Response
<pre>
Headers:
Character Encoding: utf-8
Content-Type: application/json
Data: 
  - status	状态码 0 : succ; others : error  
  - errcode	错误码
  - errmsg	错误信息
  - result	(array)
   
   [物业App查询返回]
	- stream_server_url			中心推流服务器url
	- outerbox_list  (array)
        - type                A/B 类型,名称,IP,是否为主机
        - name                    类型： 围墙机(A)，单元机(B)
        - ip
        - is_primary           0/1  是否是首要的单元机 
        
    - sentry_list (array)     岗亭机
        - name                名称,IP
        - ip  
    
   
   [室外机查询返回]
	- propcenter_ip				物业中心话机 ip
	- stream_server_url			中心推流服务器url
	- innerbox_list	 (array) 	    本单元楼的所有室内机列表
		- room_id
		- ip
	- sentry_list (array)     岗亭机
        - name                名称,IP
        - ip    
   
   [室内机查询返回]
    - stream_server_url			中心推流服务器url
    - outerbox_list  (array)
        box                 类型,名称,IP,是否为主机
                            类型： 围墙机(A)，单元机(B)
    - sentry_list (array)     岗亭机
        - name                名称,IP
        - ip  
                 
	- propcenter_ip		    物业中心话机ip
   	- net_ip				室内主机的小区网ip
  	- family_ip			    家庭网络ip
  	- room_id				房号
</pre>

#### Remarks

### 2.2 查询室内机设备列表
#### 名称:  
getInnerBoxList()

#### 描述
 
#### Request
<pre>
URL: /propserver/api/innerbox/list
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
    - type			1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App; 5:物业服务器
</pre>

#### Response
<pre>
Headers:
Character Encoding: utf-8
Content-Type: application/json
Data: 
  - status	状态码 0 : succ; others : error  
  - errcode	错误码
  - errmsg	错误信息
  - result	(array)
    - room_id           房号
    - ip                室内主机小区网地址
</pre>

#### Remarks


### 2.3 上报设备运行事件
#### 名称:  
reportEvent()

#### 描述
 不同的设备（室内app，室内主机，室外机，物业app）在运行时把关键的事件上报给服务服务器，具体的事件类型由不同的设备定义和解释。 
 
 事件： 开机、报警、错误、呼叫等等。
 
 <pre>
start - 开机启动
callin - 呼入
callout - 呼出 ,开始时间，结束时间 ，。。。
emergency - 报警
..
 </pre>
 内容包括: 时间，设备编号，事件类型，描述等等
 
#### Request
<pre>
URL: /propserver/api/device/event
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencodedn
Query Parameters:
Body:
	- dev_type 	设备类型 1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App
	- dev_id 		设备编号，室内屏设备即认证token
	- event			事件名称
	- time				时间 unix timestamp
	- content			事件内容
	- encode            content的编码方式: 
	                        plain - 纯文本 , json - json编码 
	
</pre>

#### Response
<pre>
Headers:
Character Encoding: utf-8
Content-Type: application/json
Data: 
  - status	状态码 0 : succ; others : error  
  - errcode	错误码
  - errmsg	错误信息
  
</pre>

#### Remarks

### 2.4 设备事件查询
#### 名称:  
queryDeviceEvent()

#### 描述
 
#### Request
<pre>
URL: /propserver/api/device/event
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
	- dev_type 	设备类型 1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App
	- dev_id 		设备编号
	- room_id 	房间号 （可选)
	- event		事件名称
	- start		开始时间
	- end			结束时间 ( 不提供则一天)
	
</pre>

#### Response
<pre>
Headers:
Character Encoding: utf-8
Content-Type: application/json
Data: 
  - status	状态码 0 : succ; others : error  
  - errcode	错误码
  - errmsg	错误信息
  - result
  	- dev_type 	设备类型 1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App
	- dev_id 		设备编号
	- event			事件名称
	- time				时间
	- content			事件内容
</pre>

#### Remarks



### 2.5 设备状态上报
-----
#### 名称:  

reportStatus()

#### 描述
室内屏App，室外机，物业App可设置定时上报状态信息，以便于物业中心对整个系统的监控。 

#### Request
<pre>
URL: /propserver/api/device/status
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
   - dev_type 	设备类型 1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App
	- dev_id 		设备编号
	- time  		时间 timestamp
	- ver			版本
	- start 		启动时间. timestamp 
	- elapsed 	运行时长 单位: min
	
	[室内App/室外机]
	- ip			ip地址
	- last_call_time 	最近一次呼叫时间
	- last_call_peer_id		最近一次呼叫对方的标识,房间号/楼宇号
	- last_call_peer_type	1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App
	- last_call_peer_ip
	- last_call_direction	in / out


</pre>

#### Response
<pre>
Headers:
Character Encoding: utf-8
Content-Type: application/json
Data: 
  - status	状态码 0 : succ; others : error  
  - errcode	错误码
  - errmsg	错误信息
  - result	
</pre>

#### Remarks
 
### 2.6 二维码开门转发绿城家
-----
#### 名称:  

qrOpenDoor()

#### 描述
用户在室外使用绿城家app的qr码进行扫码开门,室外机请求物业服务器S，S请求绿城家服务器验证QR码是否合法。
合法则返回okay，室外机开门。
 
#### Request
<pre>
URL: /propserver/api/outerbox/opendoor/qr
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
    - dev_type 	设备类型 1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App
	- dev_id 		设备编号
	- time  		时间 timestamp
	- qrcode        app的qr码
</pre>

#### Response
<pre>
Headers:
Character Encoding: utf-8
Content-Type: application/json
Data: 
  - status	状态码 0 : succ; others : error  
  - errcode	错误码
  - errmsg	错误信息
  - result	1: 验证成功 , 0 :失败
</pre>

#### Remarks
 
### 2.7 留影留相功能（app上传呼叫视频)
-----
#### 名称:  

videoUpload()

#### 描述
室外机呼入室内app时，app自动录像n秒，并上传物业服务器
 
#### Request
<pre>
URL: /propserver/api/innerbox/video/upload
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
    - dev_type 	设备类型 1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App
	- dev_id 		设备编号
	- file          文件内容
	
</pre>

#### Response
<pre>
Headers:
Character Encoding: utf-8
Content-Type: application/json
Data: 
  - status	状态码 0 : succ; others : error  
  - errcode	错误码
  - errmsg	错误信息
</pre>

#### Remarks
 

 
### 2.8 发送开门信息
-----
#### 名称:  

openDoor()

#### 描述
物业报警中心发送开启指定房间门禁
 
#### Request
<pre>
URL: /propserver/api/innerbox/opendoor
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
    - room_id   	房间号
	- secrect_key   报警安全随机号
	
</pre>

#### Response
<pre>
Headers:
Character Encoding: utf-8
Content-Type: application/json
Data: 
  - status	状态码 0 : succ; others : error  
  - errcode	错误码
  - errmsg	错误信息
</pre>

#### Remarks
 
 ### 2.9 连接状态监测
-----
#### 名称:  

ping()

#### 描述
设备检查与物业服务器连接是否正常
 
#### Request
<pre>
URL: /propserver/api/ping
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:    
	
</pre>

#### Response
<pre>
Headers:
Character Encoding: utf-8
Content-Type: application/json
Data: 
  - status	状态码 0 : succ; others : error  
  - errcode	错误码
  - errmsg	错误信息
</pre>

#### Remarks
 
 

