//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_CALL_H
#define INNERPROC_CALL_H

#include "connection.h"
#include "message.h"
//#include "service.h"
//#include "server.h"


class CallLineStatus{
public:

};


struct CallInfo{
	CallEndpoint	src;
	CallEndpoint	dest;
	std::shared_ptr<Connection> conn;
};

/**
 auto downcastedPtr = std::dynamic_pointer_cast<Derived>(basePtr);
  if(downcastedPtr){
  }
 */


struct CallRequest: std::enable_shared_from_this<CallRequest>{
	CallEndpoint	src;
	CallEndpoint	dest;
	std::time_t 	stime;
	Connection::Ptr conn;
	
	virtual void open(){};
	virtual void close(){
		if(conn){
			conn->close();
		}
	};
	
	virtual void keep_alive(std::time_t interval){}; // 定时发送保活包
	virtual ~CallRequest(){}
	
};

struct CallRequestIn:CallRequest{
	std::shared_ptr<MessageCallIn> msg;
	
	CallRequestIn(const std::shared_ptr<MessageCallIn> & msg_):msg(msg_){}
	
	virtual void keep_alive(std::time_t interval){}; // 检查定时存活包是否到达
	
	virtual void open(){};
	virtual void close(){};
	
	virtual void onConnected(const Connection::Ptr& conn){};
};

struct CallRequestOut:CallRequest{
	CallRequestOut(){}
	~CallRequestOut(){}
	void keep_alive(std::time_t interval){}; // 发送定时存活包
	
	void open(){}
	void close(){};
	
	void onConnected(const Connection::Ptr& conn);
};

struct CallEstablished{
	std::shared_ptr<CallRequest> req;
};





#endif //INNERPROC_CALL_H
