

# 新可视对讲系统_接口设计_物业服务器接口


Revision:

    1. 2019.1.31 scott created
	
	2019.3.13 v0.2 scott 
	
	
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



### 1. 设备初始化
-----
#### 名称:  
initDeviceParams()

#### 描述
 - 室内设备启动时向物业中心请求系统运行的参数
 - 新内屏注册时，室内主屏设备将服务器的url 转成qrcode，新设备扫描qrcode，调用 initparams 的url方法。返回的inner_token将作为室内设备访问服务器的身份凭证。内屏设备访问室内主机时也需要携带此inner_token,室内主机将校验此inner_token是否与自己的inner_token一致，方可放行。
 - token 是根据不同室内设备的编号、类型生成的唯一的设备身份标识。在之后访问物业服务器时将携带此令牌作为凭证。

#### Request
<pre>
URL: /propserver/api/initparams
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
	- token 	设备身份令牌	(室内屏设备有效)
	- id     	设备编号 
	- type   	设备类型 1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App; 5:物业服务器
	- os     	系统类型  android, ios
	- ver    	系统版本
   
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
  - result	(object)
    - server_time    		服务器时间，设备需校准本地时间    
    - propserver_url    	物业服务器地址 （室内主机端口转向）
    - stream_server_url    推流服务器地址 （室内主机端口转向）
    - net_ip             	室内主机小区网ip地址
    
    (室内屏返回参数)
    - innerbox_url       	室内主机管理访问地址    
    - room_id        		房号
    - family_ip      		室内主机家庭网地址
    - family_call_port     室内主机对讲呼叫端口
</pre>

#### Remarks



### 2.1 查询室内机设备列表
#### 名称:  
getInnerBoxList()

#### 描述
 - 室内机内屏设备从物业中心查询获取小区所有的室内机地址列表
 - 室外机设备从物业中心查询获取本栋单元楼的所有室内机地址列表

 
#### Request
<pre>
URL: /propserver/api/innerbox/list
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
   - token      设备授权身份令牌
   - type   1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App; 5:其他

   
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
    - ip                设备小区网地址
</pre>

#### Remarks


### 2.2 查询室外设备列表
-----
#### 名称:  
getOuterBoxList()

#### 描述
 - 室内机主机发起查询本单元楼的室外机信息，包括：一楼、地下一层、大门机的单元设备。

#### Request
<pre>
URL: /propserver/api/outerbox/list
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
   - token      设备授权身份令牌
   - type   1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App; 5:其他

   
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
  - result	(object)
    - gate_box           围墙机
    - f1_box             一楼单元机
    - ug_box             负一楼单元机
    - sentry_1           岗亭机1
    - sentry_2           岗亭机2
    - sentry_3           岗亭机3
    - sentry_4           岗亭机4
</pre>

#### Remarks

### 2.3 查询室内设备呼叫历史
-----
#### 名称:  
getInnerBoxCallHistoryList()

#### 描述
 - 室内设备向物业中心查询本房间的历史呼叫记录

#### Request
<pre>
URL: /propserver/api/innerbox/callhistory/list
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
   - token      设备授权身份令牌
   - type           1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App; 5:其他

   - room_id     （可选）      房号
   - start_time  （timestamp/可选）      查询开始时间，默认：当天凌晨 0:0:0
   - end_time     (timestamp/可选)      查询结束时间， 默认： 当前时间
   - event          事件类型 0x01: 呼叫进入 ， 0x02：外呼开始 , 默认类型: 0x03

* 查询不提供 room_id ,服务器将根据请求者的ip来匹配到房号   
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
    - time           呼叫时间
    - local_type     主叫设备类型
    - local_id       主叫设备编号
    - local_ip       主叫设备ip地址
    - peer_type      呼叫请求对点设备类型 
                     室内机:innerbox,室外机:outerbox, 物业中心：propserver,物业app: propapp
    - peer_id        呼叫对点设备编号 ，室内机：房号 , 其他：未定义
    - peer_ip        呼叫对点ip地址
    - event          事件类型,0x01: 呼叫进入 ， 0x02：外呼开始

</pre>

#### Remarks




### 2.5 报警上报
-----
#### 名称:  

newEmergency()

#### 描述

#### Request
<pre>
URL: /propserver/api/inner/seczone/emergency
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
   - id     设备编号 ，内屏设备可以是硬件编号
   - type   设备类型 1: 室内主机 ，2：室内屏设备   
   - port   报警端口
   - name   监控设备名称
   - message    报警消息
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
 

### 2.6 查询室内主机报警记录
-----
#### 名称:  
getEmergencyRecordList()

#### 描述
 