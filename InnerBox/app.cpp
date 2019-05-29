//
// Created by bin zhang on 2019/1/6.
//

#include "app.h"
#include "InnerController.h"
#include "version.h"
#include "base.h"

std::shared_ptr<Application>& Application::instance(){
	static std::shared_ptr<Application> handle ;
	if(!handle.get()){
		handle = std::make_shared<Application>();
	}
	return handle;
}

///

Application&  Application::init(){
	std::string version = "innerbox_"+ std::string(VERSION) + "_" + std::string(__DATE__);
	std::cout<< version << std::endl;

	cfgs_.load(SETTINGS_FILE);

	// 更新用户自定义的参数
	Config user;
	user.load("settings.user");
	cfgs_.update(user);

	logger_.setLevel(Logger::DEBUG);
	logger_.addHandler( std::make_shared<LogStdoutHandler>());
	logger_.addHandler( std::make_shared<LogFileHandler>("smartbox"));

	//开启调试输出
	if( cfgs_.get_string("debug.log.udp.enable") == "true") {
		std::string host = cfgs_.get_string("debug.log.udp.host", "127.0.0.1");
		uint16_t port = cfgs_.get_int("debug.log.udp.port", 9906);
		logger_.addHandler(std::make_shared<LogUdpHandler>(host, port));
	}

	InnerController::instance()->init(cfgs_);
	InnerController::instance()->open();
	
	return *this;
}


Logger& Application::getLogger(){
	return logger_;
}

void Application::run(){
	InnerController::instance()->run();
	wait_for_shutdown();
}

void Application::stop(){
	cv_.notify_one();
}

void Application::addService(std::shared_ptr<Service>& service ){
	services_.push_back(service);
}

Config& Application::getConfig(){
	return cfgs_;
}

void Application::wait_for_shutdown(){
	getLogger().info(name() + " started..");
	
	std::unique_lock<std::mutex> lk(mutex_);
	cv_.wait(lk);
}

std::string Application::name(){
	return cfgs_.get_string("application.name","Application");
}

int main(int argc , char ** argvs){
	Application::instance()->init().run();
}

