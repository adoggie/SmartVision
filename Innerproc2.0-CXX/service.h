//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_SERVICE_H
#define INNERPROC_SERVICE_H

#include <boost/asio.hpp>
#include "handler.h"
#include "call.h"
#include "server.h"
#include "config.h"
#include "device.h"


class Service{
protected:
	std::mutex 						mutex_;
	Config 					        cfgs_;
	boost::asio::io_service 		service_;
	boost::asio::deadline_timer 	timer_;

public:
	Service():timer_(service_){}
	virtual  ~Service(){};
public:
	virtual void lock(){};
	virtual void unlock(){};

public:
	virtual bool init(const Config& props) = 0;
	virtual bool open(){return false;};
	virtual void close(){};
	virtual void run(){}
};


class  ListenService:Service{

};


struct ServiceRegister{
	virtual void addService(std::shared_ptr<ListenService>& service ) = 0;
};



//class HttpService: Service{
//public:
//	static std::shared_ptr<InnerDeviceManager>& instance(){
//		static std::shared_ptr<InnerDeviceManager> handle ;
//		if(!handle.get()){
//			handle = new InnerDeviceManager;
//		}
//	}
//
//	bool  init(const Config& cfgs){
//		cfgs_ = cfgs;
////		return shared_from_this();
//		return true;
//	}
//
//	void addHandler(const std::shared_ptr<HttpHandler> & handler);
//};
//
//





#endif //INNERPROC_SERVICE_H
