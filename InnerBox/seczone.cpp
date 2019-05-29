#include "seczone.h"

#include <fstream>
#include <ostream>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#ifdef  BOOST_OS_LINUX
#include <sys/ioctl.h>
#endif

#include "sensor_message.h"
#include "InnerController.h"
#include "utils.h"
#include "app.h"
#include "inner_device_mgr.h"

#define DEV_GPIO "/dev/sunxi_gpio"
#define GPIO_IOC_MAGIC   'G'
#define IOCTL_GPIO_SETOUTPUT  _IOW(GPIO_IOC_MAGIC, 0, int)
#define IOCTL_GPIO_SETINPUT  _IOW(GPIO_IOC_MAGIC, 1, int)
#define IOCTL_GPIO_SETVALUE  _IOW(GPIO_IOC_MAGIC, 2, int)
#define IOCTL_GPIO_GETVALUE  _IOR(GPIO_IOC_MAGIC, 3, int)
#define GPIO_TO_PIN(bank, gpio) (32 * (bank-'A') + (gpio))

typedef struct {
	int pin;
	int data;
}am335x_gpio_arg;

seczone_conf_t g_seczone_conf[ZONE_NUMBER]; //max 16
int g_seczone_count = 0;    //实际的防区数量
//app_client_t *g_app_client_list = NULL;
char g_seczone_passwd[512];

int g_seczone_change = 0; //conf is change
//pthread_mutex_t seczone_mutex;\
int g_onekey_set = 0;

int g_seczone_mode = 0;





void SecZoneGuard::check_nurse_time()
{
//	int i = 0,j=0;
//	time_t now = 0;
//	char cmdstr[1024];
//
//	for(i=0;i<g_seczone_count;i++) {
//		if(g_seczone_conf[i].triggertype == 3 && g_seczone_conf[i].nursetime > 0) {
//			time(&now);
//			if(g_last_nursetime == 0)
//				continue;
////			DEBUG_INFO("$$$$$$ nurse:%d %d",now,g_last_nursetime);
//			if(now-g_last_nursetime > g_seczone_conf[i].nursetime) {
//				send_emergency_per10s(i);
//			}
//		}
//	}
	
}




//每10秒种报一次警
void SecZoneGuard::send_emergency_per10s(int gpio_port)
{
//	char cmdstr[1024];
//	bzero(cmdstr,1024);
//	time_t now;
//	time(&now);
//	char msg[64];
//	bzero(msg,64);
//	switch (g_seczone_conf[gpio_port].triggertype) {
//		case 0:
//			strncpy(msg,"瞬时报警",64);
//			break;
//		case 1:
//			strncpy(msg,"延时报警",64);
//			break;
//		case 3:
//			strncpy(msg,"看护报警",64);
//			break;
//
//	}
//
//	if(now-g_seczone_conf[gpio_port].etime >= 10){
//		snprintf(cmdstr,1024,"{\"method\":\"seczone_emergency\",\"params\":{\"port\":%d,\"name\":\"%s\",\"message\":\"%s\"}}",gpio_port+1,g_seczone_conf[gpio_port].name,msg);
//
//		send_cmd_to_local("127.0.0.1",cmdstr);
//		g_seczone_conf[gpio_port].etime = now;
//	}
}


/*general gpio process*/
//@gpio_port port of gpio
void SecZoneGuard::generate_emergency(int gpio_port) {
	
	time_t now;
	time(&now);
	char message[64];
	bzero(message,64);
	
	switch ( EmergencyType(g_seczone_conf[gpio_port].triggertype)) {
		//瞬时报警
		case EmergencyType::Immediately :
			send_emergency_per10s(gpio_port);
			break;
			
			//延时报警
		case EmergencyType::Delay :
			g_seczone_conf[gpio_port].delaycount++;
			if(g_seczone_conf[gpio_port].delaycount >= g_seczone_conf[gpio_port].delaytime) {
				send_emergency_per10s(gpio_port);
			}
			break;
			
			//防劫持报警
		case EmergencyType::Hijack :
			//only send to alarm center
			strncpy(message,"防劫持报警",64);
//			send_alarm_to_property(gpio_port+1,g_seczone_conf[gpio_port].name,message);
			break;
			
			//看护报警
		case EmergencyType::Nurse :
			//
//			g_last_nursetime = now;
			break;
		
	}
	
}


SecZoneGuard::SecZoneGuard(boost::asio::io_service& io_service):io_service_(io_service){

}

bool SecZoneGuard::init(const Config & cfgs){
	sec_cfgs_file_  = SECZONE_FILE;
	cfgs_ = cfgs;
	init_configs();
	load_configs(sec_cfgs_file_);	// 从文件中覆盖
	load_configs(SECZONE_USER_FILE);	// 读取用户配置数据

	PropertyStringMap sensor_props;
	sensor_props["port"] = cfgs.get_string("seczone_serial_port");
	sensor_props["baudrate"] = cfgs.get_string("seczone_serial_baudrate");
	sensor_.init(sensor_props);
	sensor_.setListener(this);

	heartbeat_interval_ = cfgs.get_int("sensor_hb_interval",5);
	max_offline_time_ = cfgs.get_int("sensor_offline_time",3600); // 默认1小时收不到心跳包
	last_heartbeat_time = std::time(NULL);
	return true;
}

bool SecZoneGuard::open(){
	sensor_.open();

	running_ = true;
	timer_ = std::make_shared<boost::asio::steady_timer>(io_service_);
	timer_->expires_after(std::chrono::seconds( heartbeat_interval_));
	timer_->async_wait(std::bind(&SecZoneGuard::check_heartbeat, this));

//	std::thread thread(std::bind(&SecZoneGuard::get_gpio_status,this));
//	std::thread thread(&SecZoneGuard::get_gpio_status,this);
	return true;
}

//离线通知 innercontroller 是否要重启设备
void SecZoneGuard::onSensorOffline(){
	InnerController::instance()->reboot();
}

// 定时发送 心跳包到 sensor
// 检查 单片机 sensor 是否心跳超时
void SecZoneGuard::check_heartbeat(){
	if( !running_ ){
		return;
	}

	if( std::time(NULL) - last_heartbeat_time > max_offline_time_ ){
		onSensorOffline();
	}

	last_heartbeat_time = std::time(NULL);

	timer_->expires_after(std::chrono::seconds( heartbeat_interval_));
	timer_->async_wait(std::bind(&SecZoneGuard::check_heartbeat, this));
}

void SecZoneGuard::close(){
	running_ = false;
}

std::string SecZoneGuard::seczonePassword(){
	return sec_cfgs_.get_string("password","1234");
}

/**
 * 与报警上报配对
 * @param rand_key 报警时生成的动态key
 */
void SecZoneGuard::openDoor(const std::string& rand_key){
	if( rand_key_ != rand_key){
		return;
	}
//    std::shared_ptr< SensorMessageOpenDoor > message = std::make_shared<SensorMessageOpenDoor>();
//    sensor_.sendMessage(message);
	std::string cmd_text = cfgs_.get_string("cmd_opendoor");
	Application::instance()->getLogger().info("send opendoor command:" + cmd_text);
	cmd_text = MessagePayload::textEncode(cmd_text);
    sensor_.sendData(cmd_text.c_str(),cmd_text.size());
	sensor_.sendSeperator();

}

void SecZoneGuard::hearbeat(){
	std::shared_ptr< SensorMessageHeartbeat > message = std::make_shared<SensorMessageHeartbeat>();
	sensor_.sendMessage(message);
}

bool SecZoneGuard::setPassword(const std::string& old,const std::string& _new ){
    Application::instance()->getLogger().debug("passwd-old:"+old + " passwd-new :" + _new + " current:"+seczone_settings_.seczone_password);
	if( seczone_settings_.seczone_password != old){
		return false;
	}
	seczone_settings_.seczone_password = _new ;
	save_configs();
	return true;
}

//设置防区参数
void SecZoneGuard::setSecZoneParams(const seczone_conf_t& conf,const std::string& passwd){
	
	int n;
	if(conf.port < 0 || conf.port >= ZONE_NUMBER){
		puts("invalid port.");

		return ;
	}
	for(n=0;n<seczone_settings_.seczone_confs.size();n++){
		auto & _ = seczone_settings_.seczone_confs[n];
		if(_.port == conf.port){
			_.name = conf.name;
			_.normalstate = conf.normalstate;
			_.nursetime = conf.nursetime;
			_.alltime = conf.alltime;
			_.triggertype = conf.triggertype;
			_.delaytime = conf.delaytime;
			_.online = conf.online;
			break;
		}
	}
	save_configs();
}

//  查询防区设置参数
Json::Value SecZoneGuard::getSecZoneParams(){
	Json::Value value;
	for( auto & cfg : seczone_settings_.seczone_confs){
		value.append( cfg.jsonValue() );
	}
	return value;
}

void SecZoneGuard::onekeySet(const std::string& passwd,bool onoff){

}

//停止报警
// 通知报警服务中心，停止报警提示
void SecZoneGuard::discardEmergency(const std::string& passwd){

}

//保存防区信息 （写入用户配置文件)
void SecZoneGuard::save_configs() {
	int n=0;

//	sec_cfgs_.save(sec_cfgs_file_);

	Config cfgs;
	std::ofstream ofs(SECZONE_USER_FILE);
	
	ofs<<"password = "<< seczone_settings_.seczone_password << std::endl;
//	ofs<<"onekeyset = "<<seczone_settings_.onekeyset << std::endl;
	
	ofs<<std::endl;

	//当前有效模式
	ofs<<"mode.num = " << seczone_settings_.seczone_mode_confs.size() << std::endl;
	ofs<<"mode.index = "<< seczone_settings_.seczone_mode_index << std::endl;

	//模式下防区开关状态
	for(n=0;n< seczone_settings_.seczone_mode_confs.size();n++){
		auto _ = seczone_settings_.seczone_mode_confs[n];
		ofs<< "mode"<<_.mode <<".value = " << _.mask << std::endl;
	}
	ofs<<std::endl;

	// 写入所有防区信息
	ofs<<"zone.num = " << seczone_settings_.seczone_confs.size() << std::endl;
	for(n=0;n< seczone_settings_.seczone_confs.size();n++){
		auto _ = seczone_settings_.seczone_confs[n];
		ofs<< std::endl;
		ofs<< "zone"<<n<<".port = " << _.port << std::endl;
		ofs<< "zone"<<n<<".name = " << _.name << std::endl;
		ofs<< "zone"<<n<<".normalstate = " << _.normalstate << std::endl;
//		ofs<< "zone"<<n+1<<".onekeyset = " << _.onekeyset << std::endl;
		ofs<< "zone"<<n<<".currentstate = " << _.currentstate << std::endl;
		ofs<< "zone"<<n<<".delaytime = " << _.delaytime << std::endl;
		ofs<< "zone"<<n<<".nursetime = " << _.nursetime << std::endl;
		ofs<< "zone"<<n<<".alltime = " << _.alltime << std::endl;
//		ofs<< "zone"<<n+1<<".gpiolevel = " << _.gpiolevel << std::endl;
		ofs<< "zone"<<n<<".triggertype = " << _.triggertype << std::endl;
		ofs<< "zone"<<n<<".online = " << _.online << std::endl;
//		ofs<< "zone"<<n<<".delaytime = " << _.delaytime << std::endl;
	}
	ofs<<std::endl;
	ofs.close();
}

bool SecZoneGuard::load_configs(const std::string& seczone_file){
	sec_cfgs_.clear();
	sec_cfgs_.load( seczone_file);
	int n;
	int num;
	std::string name;
	std::string value;
	value = sec_cfgs_.get_string("password");
	if(value!="") {
		seczone_settings_.seczone_password = value;
	}
//	seczone_settings_.onekeyset = sec_cfgs_.get_string("onekeyset");
	num = sec_cfgs_.get_int("mode.num");

	for(n=0; n < num; n++){
		seczone_settings_.seczone_mode_confs[n].mode = n;
		boost::format fmt("mode%d.value");
		fmt%(n);
		name =fmt.str();
		seczone_settings_.seczone_mode_confs[n].mask = sec_cfgs_.get_int(name );
	}

	seczone_settings_.seczone_mode_index = sec_cfgs_.get_int("mode.index",seczone_settings_.seczone_mode_index);
	
	num = sec_cfgs_.get_int("zone.num");
	
	for(n=0;n<num;n++){
		seczone_conf_t& zone = seczone_settings_.seczone_confs[n];
		name = (boost::format("zone%d.")%(n)).str();
		zone.port = n; // sec_cfgs_.get_int(name+"port");
		zone.name = sec_cfgs_.get_string(name+"name");
		zone.normalstate = sec_cfgs_.get_int(name+"normalstate");
//		zone.onekeyset = sec_cfgs_.get_string(name+"onekeyset");
		zone.currentstate = sec_cfgs_.get_int(name+"currentstate");
		zone.delaytime = sec_cfgs_.get_int(name+"delaytime");
		zone.nursetime = sec_cfgs_.get_int(name+"nursetime");
		zone.alltime = sec_cfgs_.get_int(name+"alltime");
		zone.gpiolevel = sec_cfgs_.get_int(name+"gpiolevel");
		zone.triggertype = sec_cfgs_.get_int(name+"triggertype");
		zone.online = sec_cfgs_.get_int(name+"online");
		zone.delaytime = sec_cfgs_.get_int(name+"delaytime");
		
//		if(zone.normalstate == "no"){
//			zone.gpiolevel = 0;
//		}
//		if(zone.normalstate == "nc"){
//			zone.gpiolevel = 1;
//		}
	}
	
	return true;
}

//生成默认的防区参数
void SecZoneGuard::init_configs(){


//	save_configs();
}

// 设置指定模式的参数值
void SecZoneGuard::setSecModeValue(int mode,int mask){
	if(mode < 0 || mode >= MODE_NUMBER){
		return ;
	}
	for(auto & _ : seczone_settings_.seczone_mode_confs){
		if(_.mode == mode){
			_.mask = mask;
			break;
		}
	}
	save_configs();
	
}

//接收到上行的防区设备消息
void SecZoneGuard::onMessage(std::shared_ptr<MessagePayload> &message, Sensor *sensor) {
	{
		if(message->a ==(int) MessageType::Heartbeat){
			last_heartbeat_time = std::time(NULL);
			return;
		}
	}

	// 处理报警信息
	//接收传感器上报消息
	if(message->a == (int)MessageType::SensorStatusValue){
//		if(message->d == "io"){
        if(1){
			try {
				int port = message->c;
				int value = boost::lexical_cast<int>(message->e);
				onSecZonePortStatus(port,value);
			}catch (boost::bad_lexical_cast& e){

			}
		}
	}
}

//防区状态检查
void SecZoneGuard::onSecZonePortStatus(int port,int status){
	// 监测匹配报警，触发报警
	if( port < 0 || port >= ZONE_NUMBER){
		Application::instance()->getLogger().error( "onSecZonePortStatus(),Bad Port:"+ boost::lexical_cast<std::string>(port));
		return ;
	}
	for(auto & _ : seczone_settings_.seczone_confs){
		if( _.port ==  port){
			Application::instance()->getLogger().debug("zone" + boost::lexical_cast<std::string>(port) \
													   +" normalstate:" + boost::lexical_cast<std::string>(_.normalstate));
			if( _.online == 0 ){ // 防区旁路状态，直接跳过
				Application::instance()->getLogger().debug("Zone is Offline. Signal Ignored.");
				continue;
			}
			if(_.normalstate == status ){ // 防区检测到报警
				// 检查防区在当前模式中是否打开
				seczone_mode_info_t& current = seczone_settings_.currentMode();
				Application::instance()->getLogger().debug("current mode mask:" + boost::lexical_cast<std::string>(current.mask));
				if( current.mask & (1<<port)){
					// 报警触发
					std::shared_ptr<Event_Emergency> event = std::make_shared<Event_Emergency>();
					event->port = port;
					event->name = _.name;
					event->detail = "";
					emergencyDetected(event);
				}
			}
		}
	}

}

// 监测到报警之后触发上报
void SecZoneGuard::emergencyDetected(const std::shared_ptr< Event_Emergency>& event){
    Application::instance()->getLogger().debug("emergencyDetected()..");

	if( std::time(NULL) - last_rand_key_time_ > 3600){ //有效 rand_key 保留 60 分钟
		rand_key_ = utils::generateUUID();
	}
	event->rand_key = rand_key_;

	// 1. 广播给
	std::shared_ptr< MessageEmergency > m = std::make_shared<MessageEmergency>();
	m->port = event->port;
	m->name = event->name;
	m->message = event->detail;
	InnerDeviceManager::instance()->postEmergency(m);

	InnerController::instance()->reportEvent(event);

}


Json::Value SecZoneGuard::getSecModeList(){
	Json::Value array;
	
	for(auto _ : seczone_settings_.seczone_mode_confs){
		Json::Value item;
		item = _.jsonValue();
		array.append(item);
	}
	return array;
}


int SecZoneGuard::getCurrentMode(){
	return seczone_settings_.seczone_mode_index;
}

void SecZoneGuard::setCurrentMode(int mode){
	if(mode <0 || mode >= MODE_NUMBER){
		mode =0;
	}
	seczone_settings_.seczone_mode_index = mode;
	save_configs();
}

Json::Value SecZoneGuard::getSecHistoryList(){
	return Json::Value();
}


// 本地持久化报警信息
void SecZoneGuard::save_emergency(const MessageEmergency& message){

}

//重置防区参数
void SecZoneGuard::resetParams(){
	//删除 seczone.user , 重启设备
	std::remove(SECZONE_USER_FILE);
	InnerController::instance()->reboot();
}

