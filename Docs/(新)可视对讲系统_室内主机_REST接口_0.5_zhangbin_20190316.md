

# 新可视对讲系统_接口设计


Revision:

```
2019.1.31 scott created

2019.3.13  v0.2  scott 

2019.3.16 v0.3  scott 
    1. 2.9 远程下发开门命令 (new )

2019.3.16 v0.4 scott 
    1.  主机状态 增加 seczone_mode          当前防区模式 1-4
    2. init ,增加 initdata 
2019.3.18  v0.5 scott
1. 修改防区查询、设置
2. 去除一键设防、报警撤销接口   

```

## 名词定义

* `SmartBox`      室内主机
* `MasterScreen`  室内主屏设备，工程安装在业务屋内的标准可视对讲屏设备
* `SubScreen`   室内次屏设备，加入到家庭网络的移动设备，可以是pad，手机或其他移动终端设备

## 说明
新版本系统接口设计基于旧版可视对讲系统的基础架构而进行优化和完善。

新系统特点：

1. 室外单元设备从linux主机改为Android主机，门禁交互按键功能由Android的软件实现。Android程序完成门禁消息的响应和控制，并实现单元机与室内机、单元机与中心和单元机与物业后端App的音视频对讲功能以及蓝牙、Qr码、人脸识别开门的功能。 


2. 室内实现主屏分离，由主控设备(SmartBox)和内屏设备（Screen）组成。 Screen设备网络接口和主控设备的内网接口统一接入到家庭网络端口(wifi)，在视频对话时，Screen设备可以发起对室外单元机、物业中心、对点室内机的音视频对讲，并同时能响应来自室外的音视频对讲呼入。

3. Smartbox与Screen：Screen设备通过SmartBox的认证授权之后方可接入到小区网络。Screen发起的查询请求，除了SmartBox自身处理外，其他的被转发到物业服务器处理。

4. 对讲与推流： 新系统与旧系统的推流方式不同，新系统中的推流服务器SS由对讲发起方建立，被叫方与主叫方均将自己的音视频流推送到SS进行推拉交换。


室内主机端口：

1.家庭网络

<pre>  
7890 - 管理端口，主机查询、设置等操作 (webapi)
7891 - 中心服务器推流端口 ， 室内屏App通过此端口将音视频流推送到中心服务器
7892 - 呼叫端口，室内主机建立维持的TCP呼叫链路
</pre>

2.小区网络
<pre>  
  7890 - 管理端口，主机查询、设置等操作 (webapi)
  7893 - 呼叫端口，室内外机、物业app等呼入的端口
</pre>


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

### Http 返回消息格式

```
	{ 
	  status: 0 , 	// 0 : 正常  ， 1 :异常
	  errcode: 0,  // 错误码
	  errmsg: '' , // 错误信息
	  result: {}   // 返回的数据对象，dict/array/object/简单数据类型(string,int,float)
	}
```

## 1. 设备主体功能

### 1.1 室内主机状态查询
#### 名称:  
queryInnerBoxStatus(id)

#### 描述
室内屏App和物业管理系统发起对室内主机状态的查询。 

#### Request
<pre>
URL: /smartbox/api/status
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
   - token      设备授权身份令牌
   
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
    - time              当前时间
    - ver               软件版本号
    - fds               打开文件数量
    - threads           打开线程数量
    - mem_rss           内存占用数
    - outbox_net        是否与室外机连接正常 0: offline , n: 最近此次活跃时间戳
    - propserver_net    是否与物业服务器连接正常 0: offline , n: 最近此次活跃时间戳
    - net_ip                小区网接口地址
    - net_call_port         小区网呼叫端口
    - family_ip             家庭网接口地址
    - family_call_port      家庭网呼叫端口
    - propserver_url   		 物业服务http接口地址,转发到物业服务器
    - alarm_enable          启用报警
    - watchdog_enable       启用看门狗 
  	- call_in_enable		 是否禁止呼叫进入 	0: 禁止呼入 ， 1：允许呼入	 
  	- seczone_mode          当前防区模式 1-4
</pre>

#### Remarks


### 1.2 室内主机参数设置
#### 名称:  
setInnerBoxParams()

#### 描述
设置主机运行参数
#### Request
<pre>
URL: /smartbox/api/params
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
   - token         		设备授权身份令牌
   - ip                 主机小区网ip
   - family_ip          家庭网ip
   - watchdog_enable    是否启用看门狗 0 :close , 1: open
   - alarm_enable       是否启用报警功能 0: close , 1: open
   - reboot             设备重启等待时间 0: 即刻 ，  n : 推迟秒数
   - call_in_enable		 设置是否禁止呼入 0 : 禁止 ， 1： 允许
   - save               设置参数时是否保存 0:不保存（默认) , 1:保存
   - seczone_mode          当前防区模式 0-3
   - initdata           要求设备重新加载物业服务器的配置信息             
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



### 1.3 室内设备注册请求码
#### 名称:  
getNewRequestCode()

#### 描述
室内屏发起获取新设备注册请求码 RC 

#### Request
<pre>
URL: /smartbox/api/innerscreen/reqcode
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
   - token         		设备授权身份令牌(室内屏设备注册的身份码)
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
  - result	注册请求码 
</pre>

#### Remarks


### 1.4 新设备注册
#### 名称:  
registerScreen()

#### 描述
室内新设备注册到室内主机。 室内新设备扫描室内屏的QR图形码，得到室内主机的注册url，进行注册

#### Request
<pre>
URL: /smartbox/api/innerscreen
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
	- code 		注册请求码
	- sn		设备硬件标识码
	- type		1:pad, 2: phone  
	- os 		linux,android,ios
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
  - result	(objecgt)
  	- token 设备身份码 (token)
  	- net_ip	室内主机的小区网ip
  	- family_ip 	家庭网络ip
  	- room_id 	房号

</pre>

#### Remarks



### 2. 防区设置
---------
### 2.1 设置防区密码

#### 名称:  
setSecZonePassword(passwd)
#### 描述

#### Request
<pre>
URL: /smartbox/api/seczone/passwd
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
   - token      设备授权身份令牌
   - old        当前密码
   - new        新设密码
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


### 2.2 设置防区参数

#### 名称:  
setSecZoneParams(...)
#### 描述

#### Request
<pre>
URL: /smartbox/api/seczone/params
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type:  x-www-form-urlencoded
Query Parameters:
Body:
   - token      设备授权身份令牌
   - passwd    当前密码
   - port       防区编号 0-7
   - name       防区类型， 紧急按钮，防劫持，红外，。。。
   - normalstate    正常工作状态的值 0 / 1 （非报警） 
   - triggertype    报警触发策略，数字： 0-3 (0-瞬时策略，1-延时策略，2-防劫持策略，3-看护策略)
   - nursetime      n,看护时间，triggertype=3时，有效,单位:秒，
   - delaytime      n,触发延时，triggertype=2
   - alltime        0/1, 是否为24小时防区 'yes','no'
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



### 2.3 获取设置防区参数

#### 名称:  
getSecZoneParams(...)
#### 描述
查询防区的配置信息

#### Request
<pre>
URL: /smartbox/api/seczone/params
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: application/json
Query Parameters:
   - token      设备授权身份令牌
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
    - port       防区编号
    - name       防区类型
    - normalstate    正常工作状态的值 0 / 1 （非报警） 
    - triggertype    报警触发策略，数字： 0-3 (0-瞬时策略，1-延时策略，2-防劫持策略，3-看护策略)    
    - nursetime      看护时间，triggertype=3时，有效,单位:秒，
    - delaytime      触发延时，triggertype=2
    - alltime        是否为24小时防区 (yes/no)
    
    - currentstate   当前设防状态  1:'on' 0:'off'
    - online         防区是否在线，1:在线  0：旁路

</pre>

#### Remarks


### 2.4 一键设防(作废)

#### 名称:  
setSecZoneOnekey(...)
#### 描述

#### Request
<pre>
URL: /smartbox/api/seczone/onekey
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
   - token      设备授权身份令牌
   - passwd     配置密码
   - state      on/off 启用/撤销
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




### 2.5 撤销报警（作废）
#### 名称:  
cancelEmergency()
#### 描述

#### Request
<pre>
URL: /smartbox/api/seczone/emergency
Medthod:    DELETE
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
   - token      设备授权身份令牌
   - passwd     配置密码
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



### 2.6 安防模式（设置）

#### 名称:  
setSecZoneMode(...)
#### 描述
设置指定模式下防区的启用或关闭
#### Request
<pre>
URL: /smartbox/api/seczone/mode
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
Body:
   - token      设备授权身份令牌
   - mode       模式类型 取值：0/1/2/3，分别代表四种模式
   - value      具体防区掩码值 ( 整型)  11010011  8个防区

@mode，区分不同模式，数字类型，取值：0/1/2/3，分别代表四种模式
@value，模式具体配置，格式为#隔开的字符串，on/off代表：打开/关闭
“mode”:0,
“value”:“128” 防区 7 开启 ，防区 0-6 关闭
     
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



### 2.7 安防模式（查询）

#### 名称:  
getSecZoneMode(...)
#### 描述
查询当前防区设置模式和模式相关的参数设置

#### Request
<pre>
URL: /smartbox/api/seczone/mode
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
   - token      设备授权身份令牌
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
    - mode      模式类型 取值：0/1/2/3，分别代表四种模式
                0 - 外出 ,1 - 在家 , 2 - 就寝 , 3 - 自定义
    - value     模式下具体的设置参数，8个防区的开关状态 
</pre>

#### Remarks


### 2.8 查询安防设置记录

#### 名称:  
getSecZoneHistoryRecordList(...)
#### 描述

#### Request
<pre>
URL: /smartbox/api/seczone/history/list
Medthod:    GET
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
   - token      设备授权身份令牌
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
    - time      时间 timestamp
    - type     记录类型，设置、报警、设防、撤防等
    - seczone   防区1
    - event     具体内容

</pre>

#### Remarks


### 2.9 远程下发开门命令

#### 名称:  
opendoor(...)
#### 描述

#### Request
<pre>
URL: /smartbox/api/emergency/opendoor
Medthod:    POST
Headers:   
Character Encoding: utf-8
Content-Type: x-www-form-urlencoded
Query Parameters:
   - token      设备授权身份令牌
   - rand_key   随机授权码
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

