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

class InnerController: ISensorListener, std::enable_shared_from_this<InnerController> {
	PropertyServerConnection proserver_conn_;
	OuterBoxConnection outbox_conn_;
	InnerSensor sensor_;
	WatchDog    watchdog_;
public:
	typedef std::shared_ptr<InnerController> Ptr;
	bool init(const Config& props);
	bool open();
	void close();
	void run();
	
	static std::shared_ptr<InnerController>& instance(){
		static std::shared_ptr<InnerController> handle ;
		if(!handle.get()){
			handle = new InnerController;
		}
	}
	
	void onAlarm(const std::shared_ptr<SensorAlarmInfo> alarm,ISensor* sensor);
	
	BoxDiscoverInfo getDiscoverInfo();
	BoxStatusInfo getStatusInfo();
};


#endif //INNERPROC_INNERCONTROLLER_H
