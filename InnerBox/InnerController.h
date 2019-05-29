//
// Created by bin zhang on 2019/1/11.
//

#ifndef INNERPROC_INNERCONTROLLER_H
#define INNERPROC_INNERCONTROLLER_H

#include "base.h"
#include "service.h"
#include "connection.h"
#include "sensor.h"
#include "watchdog.h"
#include "http-api.h"
#include "seczone.h"
#include <boost/asio.hpp>

#include "event.h"


class InnerController:  public std::enable_shared_from_this<InnerController> {
//	InnerSensor sensor_;

public:
    struct Settings{
        std::time_t             outbox_net;          //是否与室外机连接正常 0: offline , n: 最近此次活跃时间戳
        std::time_t             propserver_net;      //是否与物业服务器连接正常 0: offline , n: 最近此次活跃时间戳
        std::string     net_ip;              // 小区网ip
        std::uint16_t   net_call_port;
        std::string     family_ip;          // 家庭内网ip
        std::uint16_t   family_call_port;
        std::string     propserver_url;     //物业服务http接⼝地址,转发到物业服务器
        int             alarm_enable;       // 启用报警 0: 关闭 , 1: 启用
        int             watchdog_enable;    // 启⽤看⻔狗
        int             call_in_enable;     //是否禁⽌呼叫进⼊ 0:
        std::string     room_id;
        int             seczone_mode;       // 当前防区模式
        std::string     seczone_passwd;     // 防区密码


        std::string     gate_box;
        std::string     outerbox;  // 主单元机 ip地址

        std::string     propcenter_ip;

        Settings(){
            outbox_net = 0;
            propserver_net = 0;
            alarm_enable = 0;
            watchdog_enable = 0;
            call_in_enable = 0 ;
            seczone_mode = 0;
        }
    };

public:
    InnerController();
	typedef std::shared_ptr<InnerController> Ptr;
	bool init(const Config& props);
	bool open();
	void close();
	void run();
	
	static std::shared_ptr<InnerController>& instance(){
		static std::shared_ptr<InnerController> handle ;
		if(!handle.get()){
			handle = std::make_shared<InnerController>() ;
		}
		return handle;
	}
	
	void onAlarm(const std::shared_ptr<SensorAlarmInfo> alarm,Sensor* sensor);
	
//	BoxDiscoverInfo getDiscoverInfo();
//	BoxStatusInfo getStatusInfo();
	
	Json::Value getStatusInfo();

	boost::asio::io_service*  io_service(){
		return &io_service_;
	}

	std::string getDeviceUniqueID();	//获取设备编号

    void openDoor(const std::string& rand_key);    // 执行开门指令

    void setOpenDoorPassword(const std::string& password);    // 设置开门密码
    bool verifyOpenDoorPassword(const std::string& password);    //

    void setParameterValues(const PropertyStringMap& values);

    Settings& settings() { return settings_;};
    SecZoneGuard & seczone_guard() { return seczone_guard_;};

    void loadUserSettings();
    void saveUserSettings(); // 将当前配置写入 settings.user

//    void onSensorOffline(); // 防区传感器离线
    void reboot(int waitsec = 0);

    void onHttpResult(const std::string& request_name,const Json::Value & result);

    std::string getInnerIP(); //家庭网络ip地址
    std::string getOuterIP();  //设备小区网络地址
    void reportEvent(const std::shared_ptr<Event>& event);
private:

    void onInitData(const Json::Value& result);

    void httpInitData();

    void workTimedTask();

    void check_net_reachable();
private:
    std::mutex  mutex_;
	WatchDog    watchdog_;
	SecZoneGuard seczone_guard_;
	Config 		cfgs_;
	boost::asio::io_service io_service_;

    Settings    settings_;

    std::shared_ptr<boost::asio::steady_timer> timer_;
    int         check_timer_interval_;
    bool        data_inited_ ; // 指示是否从物业服务器已经获取运行参数
    std::time_t     last_check_time_ = 0;
    int         net_check_interval_ = 60 ; // 检测与物业和室外机网络连通性时间间隔

};


#endif //INNERPROC_INNERCONTROLLER_H
