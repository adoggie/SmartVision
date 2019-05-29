//
// Created by bin zhang on 2019/1/11.
//

#include "InnerController.h"
#include "inner_device_mgr.h"
#include "call_manager.h"
#include "seczone.h"
#include "http.h"
#include "version.h"
#include "utils.h"


InnerController::InnerController():seczone_guard_(io_service_){

}

bool InnerController::init(const Config& props){
	cfgs_ = props;
	if(props.get_int("watchdog.enable",0)) {
		watchdog_.init(props);
	}
	if(props.get_int("seczone.enable",1)){
		seczone_guard_.init(props);
	}
    seczone_guard_.init(props);

	HttpService::instance()->init(props);
	CallServiceManager::instance()->init(props);
	InnerDeviceManager::instance()->init(props);

    check_timer_interval_ = cfgs_.get_int("controller.check_timer",5);

    timer_ = std::make_shared<boost::asio::steady_timer>(io_service_);
    timer_->expires_after(std::chrono::seconds( check_timer_interval_));
    timer_->async_wait(std::bind(&InnerController::workTimedTask, this));



	return true;
}

bool InnerController::open(){
	if(cfgs_.get_int("watchdog.enable",0)) {
		watchdog_.open();
	}
	
	if(cfgs_.get_int("seczone.enable",1)) {
		seczone_guard_.open();
	}
    net_check_interval_ = cfgs_.get_int("net_check_interval",60);
	
	HttpService::instance()->open();
	CallServiceManager::instance()->open();
	InnerDeviceManager::instance()->open();

    reportEvent( std::make_shared<Event_SystemStart>());

	return true;
}

void InnerController::close(){
	watchdog_.close();
//	seczone_guard_.close();
	HttpService::instance()->close();
}

void InnerController::run(){
	io_service_.run();
}


void InnerController::workTimedTask(){


    httpInitData();

    // 定时检查与物业服务器和室外机的网络连通状况
    if( std::time(NULL) - last_check_time_ > net_check_interval_){
        last_check_time_ = std::time(NULL);
        check_net_reachable();
    }
    timer_->expires_after(std::chrono::seconds( check_timer_interval_));
    timer_->async_wait(std::bind(&InnerController::workTimedTask, this));
}

// todo. 确定 检测物业中心网络可达 yes
// 与室外机的网络可达，未能确定，因为存在多个室外机。 不能检查所有室外机的网络连接状态

void InnerController::check_net_reachable() {
    {
        std::string address = cfgs_.get_string("propserver_url", "http://127.0.0.1:8090");
        std::string url = (boost::format("%s/propserver/api/ping") % address.c_str()).str();
        PropertyStringMap vars;
        HttpService::instance()->http_request(url, PropertyStringMap(), vars, (void *) "check_propserver");
    }
    { // primary outbox
        if(settings_.outerbox.length()) {
            std::string url = "http://"+settings_.outerbox+":8088/api/ping";
//            std::string url = (boost::format("%s//api/ping") % address.c_str()).str();
            PropertyStringMap vars;
            HttpService::instance()->http_request(url, PropertyStringMap(), vars, (void *) "check_outbox");
        }
    }
}

//报警传入
// 将报警分发给室内接入设备和中心服务器
void InnerController::onAlarm(const std::shared_ptr<SensorAlarmInfo> alarm,Sensor* sensor){

}

// 收集设备运行状态
Json::Value InnerController::getStatusInfo(){
    BoxStatusInfo   status;
	status.time = std::time(NULL);
    status.ver = VERSION;
    status.outbox_net = (std::time(NULL) - settings_.outbox_net < net_check_interval_*2) ? 1:0 ;
    status.propserver_net = (std::time(NULL) - settings_.propserver_net < net_check_interval_*2) ? 1:0 ; ;
    status.net_ip = settings_.net_ip;
    status.net_call_port = settings_.net_call_port;
    status.family_ip = settings_.family_ip;
    status.family_call_port = settings_.family_call_port;
    status.propserver_url = settings_.propserver_url;
    status.alarm_enable = settings_.alarm_enable;
    status.watchdog_enable = settings_.watchdog_enable;
    status.call_in_enable = settings_.call_in_enable;
    status.seczone_mode = seczone_guard_.getCurrentMode();

	return status.values();
}

std::string InnerController::getDeviceUniqueID(){
    // todo. 提供获取设备硬件编码的接口
	return "112233445566";
}

// 执行开门指令 (物业会执行紧急开门指令)
void InnerController::openDoor(const std::string& rand_key){
    seczone_guard_.openDoor(rand_key);
}

// 设置设备运行参数值
void InnerController::setParameterValues(const PropertyStringMap& params){
    std::string value;
    if( params.find("watchdog_enable") != params.end()){
        value = params.at("watchdog_enable");
        if(value == "0"){
            settings_.watchdog_enable = 0;
        }else{
            settings_.watchdog_enable = 1;
        }
    }

    if( params.find("alarm_enable") != params.end()){
        value = params.at("alarm_enable");
        if(value == "0"){
            settings_.alarm_enable = 0;
        }else{
            settings_.alarm_enable = 1;
        }
    }

    // 呼入禁止 (0: 无设置允许呼入 ， 非 0 ： 延时呼入时间)
    if( params.find("call_in_enable") != params.end()){
        value = params.at("call_in_enable");
        settings_.call_in_enable = boost::lexical_cast<int>(value);
    }


    if( params.find("reboot") != params.end()){
        // do reboot
        reboot();
    }

    if( params.find("seczone_mode") != params.end() ){ //设置当前防区设置模式
        int mode = boost::lexical_cast<int>(params.at("seczone_mode"));
        seczone_guard_.setCurrentMode(mode);
    }

    if( params.find("initdata") != params.end() ){
        data_inited_ = false; // 要求重新获取
    }

    if(params.find("propserver_url")!= params.end()){
        auto url = boost::lexical_cast<std::string>(params.at("propserver_url"));
        settings_.propserver_url = url;
        saveUserSettings();
    }
}


void InnerController::loadUserSettings(){

}

// 将当前配置写入 settings.user
void InnerController::saveUserSettings(){
    Config user;
    user.set_string("propserver_url", settings_.propserver_url);
    user.save("settings.user");
}

void InnerController::reboot(int waitsec){

}

// 上报物业服务器运行事件
void  InnerController::reportEvent(const std::shared_ptr<Event>& event){
    std::string address = cfgs_.get_string("propserver_url","http://127.0.0.1:8091");
    std::string url = (boost::format("%s/propserver/api/device/event")%address.c_str() ).str();

    PropertyStringMap vars = event->httpValues();

    HttpService::instance()->http_request(url,PropertyStringMap(),vars,(void*)"reportEvent");
}


// http 返回 初始化设备 参数
// 室内主机无需查询任何信息
void InnerController::onInitData(const Json::Value& result){
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value outerbox_list = result["outerbox_list"];
    if( outerbox_list.isNull()){
        return ;
    }
    for(auto n =0 ;n< outerbox_list.size();n++){
        Json::Value _ = outerbox_list.get(n,Json::Value());
        if( _["type"].asString() == "B" && _["is_primary"].asString() == "1"){ //单元机
            std::string ip = _["ip"].asString();
            std::string name = _["name"].asString();
            settings_.outerbox = ip;
            saveUserSettings();
        }
    }

//    settings_.gate_box = result.get("gate_box","").asString();
//    settings_.f1_box = result.get("f1_box","").asString();
//    settings_.f0_box = result.get("f0_box","").asString();
//    settings_.sentry_1 = result.get("sentry_1","").asString();
//    settings_.sentry_2 = result.get("sentry_2","").asString();
//    settings_.sentry_3 = result.get("sentry_3","").asString();
//    settings_.sentry_4 = result.get("sentry_4","").asString();
//    settings_.propcenter_ip = result.get("propcenter_ip","").asString();
//    settings_.net_ip = result.get("net_ip","").asString();
//    settings_.family_ip = result.get("family_ip","").asString();
//    settings_.room_id = result.get("room_id","").asString();

    data_inited_ = true;
}

// 未初始化则进行初始化，需要定时器进行驱动
void InnerController::httpInitData(){
    if( data_inited_) {
        return ;
    }
//    data_inited_ = true; // todo .for debug & test , remove later.

    std::string address = cfgs_.get_string("propserver_url","http://127.0.0.1:8091");
    std::string url = (boost::format("%s/propserver/api/initdata?type=%d")%address.c_str()% int(CallPeerType::INNER_BOX) ).str();

    PropertyStringMap vars;
    HttpService::instance()->http_request(url,PropertyStringMap(),vars,(void*)"initdata");
}

//http client 请求返回数据处理
void InnerController::onHttpResult(const std::string& request_name,const Json::Value & root){

    Json::Value result = root.get("result",Json::Value());

    if( request_name == "initdata"){
        onInitData(result);
    }else if( request_name == "check_propserver"){
        settings_.propserver_net = (std::time_t) std::time(NULL);
    }else if(request_name == "check_outbox"){
        settings_.outbox_net = (std::time_t) std::time(NULL);
    }

}

//家庭网络ip地址
std::string InnerController::getInnerIP(){
    std::string ip  = cfgs_.get_string("family_ip");
    if(ip.length() == 0){
        //获取实际的内网地址
        std::string nic  = cfgs_.get_string("nic_1","eth0");
        ip = utils::getIpAddress(nic);
    }
    return ip;
}

//设备小区网络地址
std::string InnerController::getOuterIP(){
    std::string ip  = cfgs_.get_string("call_listen_ip");
    if(ip.length() == 0){
        //获取实际的外网地址
        std::string nic  = cfgs_.get_string("nic_1","eth1");
        ip = utils::getIpAddress(nic);
    }
    return ip;
}

//void ev_handler(struct mg_connection *c, int ev, void *p)

void InnerController::setOpenDoorPassword(const std::string& password){

}

bool InnerController::verifyOpenDoorPassword(const std::string& password){
    return true;
}