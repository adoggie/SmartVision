//
// Created by bin zhang on 2019/1/11.
//

#include "InnerController.h"


bool InnerController::init(const Config& props){
	watchdog_.init(props);
	sensor_.init(props);
	return true;
}

void InnerController::open(){
	proserver_conn_.setListener(std::shared_from_this());
	outbox_conn_.setListener(std::shared_from_this());
	watchdog_.open();
	sensor_.open();
}

void InnerController::close(){

}

void InnerController::run(){

}

//报警传入
// 将报警分发给室内接入设备和中心服务器
void InnerController::onAlarm(const std::shared_ptr<SensorAlarmInfo> alarm,ISensor* sensor){

}