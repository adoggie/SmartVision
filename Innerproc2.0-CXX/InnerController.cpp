//
// Created by bin zhang on 2019/1/11.
//

#include "InnerController.h"
#include "inner_device_mgr.h"
#include "call_manager.h"
#include "seczone.h"
#include "http.h"

bool InnerController::init(const Config& props){
	cfgs_ = props;
	if(props.get_bool("smartbox.watchdog.enable")) {
		watchdog_.init(props);
	}
	if(props.get_bool("smartbox.seczone.enable")){
//		seczone_guard_.init(props);
	}
	
	HttpService::instance()->init(props);
	CallServiceManager::instance()->init(props);
	InnerDeviceManager::instance()->init(props);
	return true;
}

bool InnerController::open(){
	if(cfgs_.get_bool("smartbox.watchdog.enable")) {
		watchdog_.open();
	}
	
	if(cfgs_.get_bool("smartbox.seczone.enable")) {
//		seczone_guard_.open();
	}
	
	HttpService::instance()->open();
	CallServiceManager::instance()->open();
	InnerDeviceManager::instance()->open();
	return true;
}

void InnerController::close(){
	watchdog_.close();
//	seczone_guard_.close();
	HttpService::instance()->close();
}

void InnerController::run(){

}

//报警传入
// 将报警分发给室内接入设备和中心服务器
void InnerController::onAlarm(const std::shared_ptr<SensorAlarmInfo> alarm,ISensor* sensor){

}


Json::Value InnerController::getStatusInfo(){
	BoxStatusInfo status;
	status.time = std::time(NULL);
	return status.jsonValues();
}