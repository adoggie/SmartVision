//
// Created by bin zhang on 2019/2/5.
//

#ifndef INNERPROC_SECZONE_H
#define INNERPROC_SECZONE_H

#include "base.h"
#include <stdio.h>
#include <time.h>
#include <thread>
#include <jsoncpp/json/json.h>
#include <boost/asio.hpp>

#include "config.h"
#include "message.h"
#include "sensor.h"
#include "event.h"

#define ZONE_NUMBER 8
#define MODE_NUMBER 4

enum  class EmergencyType{
	Immediately = 0 , //即刻
	Delay = 1 , 		//延时报警
	Hijack = 2 , 	//防劫持报警
	Nurse = 3 		// 看护报警
};

enum class SecZoneNormalState{
	Open = 1,
	Close = 2
};

// 防区信息
typedef struct seczone_conf {
	int 			port;				//防区编号 0 - n
	std::string 	name;				//防区类型名称
//	std::string 	normalstate;		//传感器类型，nc为常闭，no为常开
	int 			normalstate;		//正常工作状态下的值 0/1
	int 			nursetime;			//180000,单位秒，看护时间，triggertype=3时，有效
	int 			alltime;			//“yes”//是否为24小时防区
	int 			triggertype;		//报警触发策略，数字： 0-3 (0-瞬时策略，1-延时策略，2-防劫持策略，3-看护策略)
	int 	currentstate;		// 0: 旁路 ， 1 ： 当前设防状态  'on' 'off'
	int 	online;				//	防区是否在线，为no时代表旁路 (yes/no)

	int 			gpiolevel; 			//报警触发点评
	time_t 			etime;
	time_t 			lastfailtime;

	int 			delaytime;				//30,   //单位秒，延时时间，与triggertype配置使用，为1时有效
	int 			delaycount;

	seczone_conf(){
		port = 0;
		delaytime = 0;
		delaycount = 0;
		nursetime = 0;
		triggertype = 0;
		gpiolevel = 0;
		etime = 0;
		lastfailtime = 0;
		alltime = 0 ;
		online = 0 ;  // 默认旁路， 不启用
	}

	Json::Value jsonValue(){
		Json::Value value;
		value["port"] = port;
		value["name"] = name;
		value["normalstate"] = normalstate;
		value["triggertype"] = triggertype;
//		value["onekeyset"] = onekeyset;
		value["currentstate"] = currentstate;
		value["nursetime"] = nursetime;
		value["online"] = online;
		value["alltime"] = alltime;
		value["delaytime"] = delaytime;
		return value;

	}
} seczone_conf_t;


typedef struct seczone_mode_info{
	int 		mode;			// 模式 编号  0-3
//	std::string value;	// 名称
	int 		active;			// 是否是当前模式

	int 		mask;		// bit mask , 防区起效掩码

//	void 	setValue(const std::string& value_){
//
//	}

	Json::Value jsonValue() {
		Json::Value value;
		value["mode"] = mode;
		value["value"] = mask;
		return value;
	}
}seczone_mode_info_t;

// 防区管理信息
typedef struct seczone_settings{
	std::string 						seczone_password;	// 防区设置密码
	std::vector<seczone_conf_t> 		seczone_confs; 		// 防区列表
	std::vector<seczone_mode_info_t> 	seczone_mode_confs; 	// 模式设置参数
	std::atomic_int 					seczone_mode_index;	// 当前模式

	seczone_mode_info_t& currentMode(){
		int index = 0;
		if(seczone_mode_index >=0 && seczone_mode_index < seczone_mode_confs.size()){
			index = seczone_mode_index;
		}
		return seczone_mode_confs.at(index);
	}

    seczone_settings(){
        int n;
        for(n=0;n<ZONE_NUMBER;n++){
			seczone_conf_t conf;
			conf.port = n;
            seczone_confs.push_back(conf);
        }

        for(n=0;n<MODE_NUMBER;n++){
			seczone_mode_info_t mode;
			mode.mode = n;
            seczone_mode_confs.push_back(mode);
        }
        seczone_password = "0000";
        seczone_mode_index = 0 ;
	}

}seczone_settings_t;

//紧急报警接收
struct IEmergencyListener{
	virtual void onEmergency(const MessageEmergency& message) = 0;
};

typedef std::vector<IEmergencyListener*> EmergencyListenterList;

class SecZoneGuard:public ISensorListener{
public:
	SecZoneGuard( boost::asio::io_service& io_service);
	bool init(const Config & cfgs);
	bool open();
	void close();
	bool setPassword(const std::string& old,const std::string& _new );
	std::string seczonePassword();
	void setSecZoneParams(const seczone_conf_t& conf,const std::string& passwd); // 设置防区参数
	Json::Value getSecZoneParams(); // 获取防区参数
	void onekeySet(const std::string& passwd,bool onoff);		// 一键设防
	void discardEmergency(const std::string& passwd);		// 撤销报警
	void setSecModeValue(int mode,int mask);		//设置模式以及相应的参数
	int getCurrentMode();
	void setCurrentMode(int mode);
	Json::Value getSecModeList();			//获取模式参数
	Json::Value getSecHistoryList();		//查询安防设置记录
	void onMessage(std::shared_ptr<MessagePayload> &payload, Sensor *sensor) ; // 接收到上行消息

	void openDoor(const std::string& rand_key);
	void hearbeat();    //

    void resetParams();
private:
	void check_nurse_time();
	void generate_emergency(int gpio_port);
	void send_emergency_per10s(int gpio_port);
	void get_gpio_status();
	
	void init_configs();
	bool load_configs(const std::string& seczone_file);
	void save_emergency(const MessageEmergency& message);
	
	void save_configs();

	void check_heartbeat();
	void onSensorOffline();	// 传感器离线

	void onSecZonePortStatus(int port,int status); // 采集 防区端口电平值

	void emergencyDetected(const std::shared_ptr< Event_Emergency> & event); //报警触发
private:
//	std::vector<seczone_mode_info_t> sec_modes_;
	std::recursive_mutex mutex_;
	Config sec_cfgs_;
	std::string sec_cfgs_file_;
	seczone_settings_t seczone_settings_;
	Sensor 	sensor_;
	std::shared_ptr<boost::asio::steady_timer> 	timer_;
	boost::asio::io_service & io_service_;

	int heartbeat_interval_;
	int max_offline_time_;			// sensor 模块最大检测离线时间
	std::time_t  last_heartbeat_time;

	std::atomic_bool running_;

	std::string     rand_key_;  //控制开门的密码
    std::time_t     last_rand_key_time_;    // 最新生成randkey的时间
	Config  cfgs_;
};
#endif //INNERPROC_SECZONE_H
