//
// Created by bin zhang on 2019/1/6.
//

#include "app.h"


std::shared_ptr<Application>& Application::instance(){
	static std::shared_ptr<Application> handle ;
	if(!handle.get()){
		handle = new Application;
	}
}

///

Application&  Application::init(){
	return *this;
}


Logger& Application::getLogger(){
	return _logger;
}

void Application::run(){

}

void Application::stop(){

}

void Application::addService(std::shared_ptr<ListenService>& service ){

}

Config& Application::getConfig(){
	return _cfgs;
}


void Application::wait_for_shutdown(){

}



int main(int argc , char ** argvs){
	Application app;
	app.init().run();
}

