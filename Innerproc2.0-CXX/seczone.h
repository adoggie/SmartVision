//
// Created by bin zhang on 2019/2/5.
//

#ifndef INNERPROC_SECZONE_H
#define INNERPROC_SECZONE_H

#include "base.h"
#include <stdio.h>
#include <time.h>
#include <thread>

#include "config.h"
#include "message.h"

#define ZONE_NUMBER 16
#define MODE_NUMBER 4

enum  class EmergencyType{
	Immediately = 0 , //即刻
	Delay = 1 , 		//延时报警
	Hijack = 2 , 	//防劫持报警
	Nurse = 3 		// 看护报警
};

typedef struct seczone_conf {
	int port;
	std::string name;			//燃气”//防区类型
	std::string normalstate;	//燃气”//防区类型
	std::string onekeyset;		//是否参与一键设防  忽略
	std::string currentstate;	//“on”,//当前设防状态  忽略
	int delaytime;				//30,   //单位秒，延时时间，与triggertype配置使用，为1时有效
	int delaycount;
	int nursetime;			//180000,//单位秒，看护时间，triggertype=3时，有效
	std::string alltime;		//“yes”//是否为24小时防区
	int triggertype;		//报警类型 //报警触发策略，数字： 0-3
	std::string online;			//:“yes”, //防区是否在线，为no时代表旁路
	int gpiolevel; 			//报警触发点评
	time_t etime;
	time_t lastfailtime;
	seczone_conf(){
		port = 0;
		delaytime = 0;
		delaycount = 0;
		nursetime = 0;
		triggertype = 0;
		gpiolevel = 0;
		etime = 0;
		lastfailtime = 0;
	}
} seczone_conf_t;


typedef struct seczone_mode_info{
	int mode;
	std::string value;
	int active;
}seczone_mode_info_t;

typedef struct seczone_settings{
	std::string seczone_password;
	std::vector<seczone_conf_t> seczone_confs; // [ZONE_NUMBER];
	std::vector<seczone_mode_info_t> seczone_mode_confs; //[MODE_NUMBER];
	volatile  int seczone_mode_index;
	std::string onekeyset;		//
}seczone_settings_t;

//紧急报警接收
struct IEmergencyListener{
	virtual void onEmergency(const MessageEmergency& message) = 0;
};

typedef std::vector<IEmergencyListener*> EmergencyListenterList;

class SecZoneGuard{
public:
	SecZoneGuard();
	bool init(const Config & cfgs);
	bool open();
	void close();
	bool setPassword(const std::string& old,const std::string& _new );
	std::string seczonePassword();
	void setSecZoneParams(const seczone_conf_t& conf,const std::string& passwd); // 设置防区参数
	Json::Value getSecZoneParams(); // 获取防区参数
	void onekeySet(const std::string& passwd,bool onoff);		// 一键设防
	void discardEmergency(const std::string& passwd);		// 撤销报警
	void setSecModeValue(int mode,const std::string& value);		//设置模式以及相应的参数
	int getCurrentMode();
	void setCurrentMode(int mode);
	Json::Value getSecModeList();			//获取模式参数
	Json::Value getSecHistoryList();		//查询安防设置记录
	
private:
	void check_nurse_time();
	void generate_emergency(int gpio_port);
	void send_emergency_per10s(int gpio_port);
	void get_gpio_status();
	
	void init_configs();
	bool load_configs();
	void save_emergency(const MessageEmergency& message);
	
	void save_configs();
private:
//	std::vector<seczone_mode_info_t> sec_modes_;
	std::recursive_mutex mutex_;
	Config sec_cfgs_;
	std::string sec_cfgs_file_;
	seczone_settings_t seczone_settings_;
	
};
#endif //INNERPROC_SECZONE_H
