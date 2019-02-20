//
// Created by bin zhang on 2019/1/10.
//

#include "call_manager.h"
#include <jsoncpp/json/json.h>
#include "message.h"
#include "inner_device_mgr.h"
#include "InnerController.h"
#include "app.h"



CallServiceManager::CallServiceManager():deadline_timer_(io_service_){

}

bool CallServiceManager::init(const Config& cfg){
	check_status_interval_ = (std::size_t) cfg.get_int("callmgr.check_status_interval",5); // 定时扫描呼叫状态
	auto address = cfg.get_string("callmgr.server_ip","127.0.0.1");
	auto port = cfg.get_int("callmgr.server_port",17809);
	
	server_ = std::make_shared<SocketServer>(io_service_,address,(unsigned short)port);
	server_->setListener(this);
	return true;
}

bool CallServiceManager::open(){
//	deadline_timer_.expires_after(std::chrono::seconds(check_status_interval_));
//	deadline_timer_.async_wait(std::bind(&CallServiceManager::check_status, this));
	server_->setListener(this);
	server_->start();
	return true;
}

void CallServiceManager::close(){
	Service::close();
}

// 检查 in/out 连接上是否连续发送接收到 call_keep消息，并没有超时
void CallServiceManager::check_status() {
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	
	if( live_time_inside_ || live_time_outside_) {
		if (std::time(nullptr) - live_time_inside_ > check_status_interval_ ||
			std::time(nullptr) - live_time_outside_ > check_status_interval_) {
			InnerDeviceManager::instance()->endCall();
			if (callreq_) { // 此为进入连接或向外连接
				callreq_->conn->send(MessageCallEnd().marshall()); // 通话结束
				callreq_->conn->close();
				callreq_.reset(); //  清除当前无连接进入或外出中
			}
			live_time_outside_ = 0;
			live_time_inside_ = 0;
		}else{
			//再次启动定时器检查
			deadline_timer_.expires_after(std::chrono::seconds(check_status_interval_));
			deadline_timer_.async_wait(std::bind(&CallServiceManager::check_status, this));
		}
	}
	
}

	
void CallServiceManager::run(){
//	thread_ = std::make_shared<std::thread>( std::bind(&CallServiceManager::thread_run,this) );
	Service::run();
}

void CallServiceManager::onConnected(const Connection::Ptr& conn){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	
	if(callreq_) {
		deadline_timer_.expires_after(std::chrono::seconds(check_status_interval_));
		deadline_timer_.async_wait(std::bind(&CallServiceManager::check_status, this));
		
	}
	
	
}

//外部连接丢失
void CallServiceManager::onDisconnected(const Connection::Ptr& conn){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	if(callreq_){
		callreq_.reset();
	}
	InnerDeviceManager::instance()->endCall();
	live_time_inside_ = live_time_outside_ = 0;
}

void CallServiceManager::onData(const boost::asio::streambuf& buffer,const Connection::Ptr& conn){

}

//对外连接失败
void CallServiceManager::onConnectError(const Connection::Ptr& conn,ConnectionError error){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	if(callreq_){
		callreq_->close();
		callreq_.reset();
	}
	InnerDeviceManager::instance()->rejectCall();
	live_time_inside_ = live_time_outside_ = 0;
	
}

void CallServiceManager::onJsonText(const std::string & text,const Connection::Ptr& conn){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	
	Message::Ptr message = MessageJsonParser::parse(text.c_str(),text.size());
	if(!message){
		return ;
	}
	
	{
		std::shared_ptr<MessageCallIn> msg = std::dynamic_pointer_cast<MessageCallIn>(message);
		if (msg) {
			onCallIn(msg, conn);
		}
	}
	

	{ //对外呼叫
		std::shared_ptr<MessageCallAccept> msg = std::dynamic_pointer_cast<MessageCallAccept>(message);
		if(msg) onCallAccept(msg,conn);
	}
	
	{ //对外呼叫
		std::shared_ptr<MessageCallReject> msg = std::dynamic_pointer_cast<MessageCallReject>(message);
		if(msg) onCallReject(msg,conn);
	}
	
	
	{ //对外呼叫
		std::shared_ptr<MessageCallEnd> msg = std::dynamic_pointer_cast<MessageCallEnd>(message);
		if(msg) onCallEnd(msg,conn);
	}
	
	{ //对外呼叫
		std::shared_ptr<MessageCallKeep> msg = std::dynamic_pointer_cast<MessageCallKeep>(message);
		if(msg) onCallKeep(msg,conn);
	}
	
}

void CallServiceManager::onCallKeep(const std::shared_ptr<MessageCallKeep>  & msg ,const Connection::Ptr& conn){
	//呼叫进入，内屏设备未接听时，外部设备连续发送call_keep,需将call_keep 连续发送给多个内部设备
	auto retmsg = InnerDeviceManager::instance()->keepCall();
}

//外部返回呼叫拒绝
void CallServiceManager::onCallReject(const std::shared_ptr<MessageCallReject>  & msg ,const Connection::Ptr& conn){
	if(callreq_){
		callreq_->close();
		callreq_.reset();
	}
	InnerDeviceManager::instance()->rejectCall();
	live_time_inside_ = live_time_outside_ = 0;
}

void CallServiceManager::onCallAccept(const std::shared_ptr<MessageCallAccept>  & msg ,const Connection::Ptr& conn){
	auto retmsg = InnerDeviceManager::instance()->acceptCall();
	auto msgend= std::dynamic_pointer_cast<MessageCallEnd>(retmsg);
	if(msgend){
		if(callreq_){
			callreq_->close();
			callreq_.reset();
		}
		live_time_inside_ = live_time_outside_ = 0;
	}
}

void CallServiceManager::onCallIn(const std::shared_ptr<MessageCallIn> & msg,const Connection::Ptr& conn){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	
	if(callreq_){
		// return LineBusy
		conn->send(MessageCallReject(MessageCallReject::Reason::line_busy,"line_busy").marshall());
		conn->close();
		return ;
	}
	std::shared_ptr<CallRequestIn> reqin = std::make_shared<CallRequestIn>(msg);
	reqin->conn = conn;
	callreq_ = reqin;
	callreq_->open();
	
	//启动定时器检测状态
	deadline_timer_.expires_after(std::chrono::seconds(check_status_interval_));
	deadline_timer_.async_wait(std::bind(&CallServiceManager::check_status, this));
	
}

void CallServiceManager::onCallEnd(const std::shared_ptr<MessageCallEnd> & msg,const Connection::Ptr& conn){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	if(callreq_){ // 通知 InnerDeviceManager->
		InnerDeviceManager::instance()->endCall();
		callreq_->close();
		callreq_.reset();
		live_time_inside_ = live_time_outside_ = 0;
	}
	
}

// 呼入或呼出的请求结束 , 原因可以是主动挂断，或网络链接丢失
void CallServiceManager::endCall(){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	if(callreq_){
		callreq_->conn->send(MessageCallEnd().marshall());
		callreq_->conn->close();
		callreq_.reset();
	}
}

std::shared_ptr<Message> CallServiceManager::keepCall(){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	live_time_inside_ = std::time(nullptr);
	if(callreq_){
		callreq_->conn->send(MessageCallKeep().marshall());
		return std::shared_ptr<Message>();
	}
	// line of outside is lost.
	return std::make_shared<MessageCallEnd>();
}

void CallServiceManager::rejectCall(){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	if(callreq_){
		callreq_->conn->send(MessageCallReject().marshall());
		callreq_->conn->close();
		callreq_.reset();
	}
}

std::shared_ptr<Message> CallServiceManager::acceptCall(){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	if(callreq_){
		callreq_->conn->send(MessageCallAccept().marshall());
		return std::shared_ptr<Message>();
	}
	return std::shared_ptr<MessageCallEnd>(); // drop it
}

//void CallServiceManager::sendMessageOnCalling(const Message::Ptr& msg){
//	{
//		std::shared_ptr<MessageCallKeep> m = std::dynamic_pointer_cast<MessageCallKeep>(msg);
//		if(m){
//			live_time_inside_ = std::time(nullptr);
//		}
//	}
//}

bool CallServiceManager::callOut(const CallInfo& info){
	std::lock_guard<std::recursive_mutex> lock(this->mutex_);
	
	if(callreq_){
		return false;
	}
	
	std::shared_ptr<CallRequestOut> callout = std::make_shared<CallRequestOut>();
	callout->src = info.src;
	callout->dest = info.dest;
	callout->stime = std::time(nullptr);
	
	// do something..
	Connection::Ptr conn = std::make_shared<Connection>(server_->get_io_service());
	conn->setListener(this);
	
	callout->conn = conn;
	
	callreq_ = callout;
	
	Config& cfgs = Application::instance()->getConfig();
	auto ip = callout->dest.ip;
	auto port = callout->dest.port;
	boost::asio::ip::address address = boost::asio::ip::make_address(ip);
	boost::asio::ip::tcp::endpoint ep(address,port);
	conn->startConnect(ep);
	
//	deadline_timer_.expires_after(std::chrono::seconds(check_status_interval_));
//	deadline_timer_.async_wait(std::bind(&CallServiceManager::check_status, this));
	
	return true;
}



void CallServiceManager::setCallInDenied(int delay){
	callin_denied_delay_ = delay;
}

int CallServiceManager::callinDenied(){
	return callin_denied_delay_;
}