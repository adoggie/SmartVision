#ifndef INNERPROC_WATCHDOG_H
#define INNERPROC_WATCHDOG_H

#include "base.h"
#include "config.h"

//传感器控制类
class WatchDog{

public:
	bool init(const Config& props);
	void open();
	void close();
};



#endif