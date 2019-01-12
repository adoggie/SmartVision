//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_HTTP_H
#define INNERPROC_HTTP_H

#include <thread>
#include <atomic>
#include "base.h"
#include "service.h"
#include "mongoose.h"

struct HttpRequest{
	std::string url;
	PropertyMap head;
	std::string method;
	std::string content;
};

struct HttpResponse{

};


class HttpService:Service{

public:
	static std::shared_ptr<HttpService>& instance(){
		static std::shared_ptr<HttpService> handle ;
		if(!handle.get()){
			handle = std::make_shared<HttpService>() ;
		}
	}
	
	bool  init(const Config& cfgs);
	
	bool open();
	void close();
	void run();
private:
	void ev_handler(struct mg_connection *nc, int ev, void *ev_data);
	void thread_run();
	
	std::shared_ptr<std::thread> thread_;
	std::atomic_bool running_;
};

//class InnerServiceHandler:HttpHandler{
//
//	void onRequest(const std::shared_ptr<HttpRequest>& req,std::shared_ptr<HttpResponse>& resp);
//
//};



#endif //INNERPROC_HTTP_H
