#ifndef INNERPROC_WATCHDOG_H
#define INNERPROC_WATCHDOG_H

#include <thread>
#include "base.h"
#include "config.h"


//传感器控制类
class WatchDog{

public:
	bool init(const Config& props);
	bool open();
	void close();
	void pause();
	void resume();
private:
	void workThread();
	void watchdog_kick();
private:
	bool running_ = false;
	int watchdogfd_ = -1;
	bool paused_ = false;
	std::shared_ptr<std::thread> thread_;
	
	int kick_freq_ = 0 ;
};



#endif