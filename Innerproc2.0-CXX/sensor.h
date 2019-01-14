#ifndef INNERPROC_SENSOR_H
#define INNERPROC_SENSOR_H

#include "base.h"
#include "config.h"

struct ISensor{

};


struct SensorAlarmInfo{

};

struct ISensorListener{
	virtual void onAlarm(const std::shared_ptr<SensorAlarmInfo> alarm,ISensor* sensor)=0;
};


//传感器控制类
class InnerSensor:ISensor{
public:
	bool init(const Config& props);
	void open();
	void close();
};



#endif