//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_APP_H
#define INNERPROC_APP_H

#include "base.h"
#include "service.h"
#include "config.h"
#include "InnerController.h"

class Logger{
	void debug();

};

class Application:ServiceObject,ServiceRegister{
	Logger 	logger_;
	Config  cfgs_;
	
	InnerController controller_;
public:
	static std::shared_ptr<Application>& instance();

	Application&  init();

	Logger& getLogger(){
		return logger_;
	}

	void run();
	void stop();

	void addService(std::shared_ptr<ListenService>& service );

	Config& getConfig(){
		return cfgs_;
	}
	
	InnerController& getController(){
		return controller_;
	}

protected:
	void wait_for_shutdown();

};



#endif //INNERPROC_APP_H
