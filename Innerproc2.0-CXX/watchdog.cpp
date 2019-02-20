#include "watchdog.h"
#include <fcntl.h>
#include <sys/ioctl.h>

#ifndef _MACOS
	#include <linux/watchdog.h>
	
#else
	#define WDIOS_DISABLECARD 1
	#define WDIOC_SETOPTIONS 1
	#define WDIOC_KEEPALIVE 1
#endif



bool WatchDog::init(const Config& props){
	kick_freq_ = 3;
	return true;
}

bool WatchDog::open(){
	watchdogfd_ = ::open("/dev/watchdog", O_RDWR);
	if (watchdogfd_ < 0) {
		printf("Unable to open /dev/watchdog\n");
		return false;
	}
	thread_ = std::make_shared<std::thread>(std::bind(&WatchDog::workThread,this));
	return true;
}

void WatchDog::workThread(){
	running_ = true;
	while(running_) {
		if(!paused_) {
			watchdog_kick();
		}
		std::this_thread::sleep_for(std::chrono::seconds(kick_freq_));
	}
}

void WatchDog::close(){
	int flags = WDIOS_DISABLECARD;
	
	running_ = false;
	thread_->join();
	
	int result = ioctl(watchdogfd_, WDIOC_SETOPTIONS, &flags);
	if (result < 0) {
		printf("Unable to stop watchdog\n");
	}
	::close(watchdogfd_);
}

void WatchDog::pause(){
	paused_ = true;
}

void WatchDog::resume(){
	paused_ = false;
}


/* 喂狗函数 */
void WatchDog::watchdog_kick(){
	int result = 0;
	result = ioctl(watchdogfd_, WDIOC_KEEPALIVE);
	
	if (result < 0) {
		printf("Unable to kick watchdog\n");
	}
}
