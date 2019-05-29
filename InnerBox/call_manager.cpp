//
// Created by bin zhang on 2019/1/10.
//

#include "call_manager.h"
#include <jsoncpp/json/json.h>
#include "message.h"
#include "inner_device_mgr.h"
#include "InnerController.h"
#include "app.h"



CallServiceManager::CallServiceManager():deadline_timer_(io_service_),work_timer_(io_service_){

}

bool CallServiceManager::init(const Config& cfg){
	cfgs_ = cfg;
	live_time_inside_ = live_time_outside_ = 0;
	callin_denied_delay_ = 0;

	check_status_interval_ = (std::size_t) cfg.get_int("callmgr.check_status_interval",5); // 定时扫描呼叫状态
	auto address = cfg.get_string("call_listen_ip","127.0.0.1");
	auto port = cfg.get_int("call_listen_port",17809);
	address = InnerController::instance()->getOuterIP();

	server_ = std::make_shared<SocketServer>(*InnerController::instance()->io_service(),address,(unsigned short)port);
	server_->setListener(this);

	return true;
}

bool CallServiceManager::open(){
//	deadline_timer_.expires_after(std::chrono::seconds(check_status_interval_));
//	deadline_timer_.async_wait(std::bind(&CallServiceManager::check_status, this));
	server_->setListener(this);
	server_->start();

	work_timer_.expires_after(std::chrono::seconds(work_interval_));
	work_timer_.async_wait(std::bind(&CallServiceManager::work_timer, this));

	if( cfgs_.get_string("keepalive_check.enable")!="false") {
		deadline_timer_.expires_after(std::chrono::seconds(check_status_interval_));
		deadline_timer_.async_wait(std::bind(&CallServiceManager::check_status, this));

	} else{
		Application::instance()->getLogger().info("keepalive_check is closed.");
	}

	return true;
}

// 线程主动检查是否已设置呼入禁止
void CallServiceManager::work_timer(){
	if( callin_denied_delay_ >0 ){
		callin_denied_delay_ -= work_interval_;
	}
	if(callin_denied_delay_ < 0){
		callin_denied_delay_ = 0 ;
	}
	work_timer_.expires_after(std::chrono::seconds(work_interval_));
	work_timer_.async_wait(std::bind(&CallServiceManager::work_timer, this));

}

void CallServiceManager::close(){
	Service::close();
}

// 检查 in/out 连接上是否连续发送接收到 call_keep消息，并没有超时
void CallServiceManager::check_status() {
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
	
	if( live_time_inside_ || live_time_outside_) {
		if (std::time(nullptr) - live_time_inside_ > check_status_interval_ ||
			std::time(nullptr) - live_time_outside_ > check_status_interval_) {

			Application::instance()->getLogger().info("call keep timeout, close connection..");
			InnerDeviceManager::instance()->endCall();
			if (callreq_) { // 此为进入连接或向外连接
				callreq_->conn->send(MessageCallEnd().marshall()); // 通话结束
				callreq_->conn->close();
				callreq_.reset(); //  清除当前无连接进入或外出中
			}
			live_time_outside_ = 0;
			live_time_inside_ = 0;
		}
	}
	//再次启动定时器检查
	deadline_timer_.expires_after(std::chrono::seconds(check_status_interval_));
	deadline_timer_.async_wait(std::bind(&CallServiceManager::check_status, this));
	
}

	
void CallServiceManager::run(){
//	thread_ = std::make_shared<std::thread>( std::bind(&CallServiceManager::thread_run,this) );
	Service::run();
}


//处理外呼连接和内呼连接
// 内呼只允许一路连接，当前未退出时拒绝一切进入的连接
void CallServiceManager::onConnected(const Connection::Ptr& conn){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);

	Application::instance()->getLogger().debug("CallService Connection Established!");

	if( conn->server() ){ //  call incoming
		if( callreq_ ){
			// 呼入时已经有呼叫成立，则拒绝
			conn->userdata((void*)"reject");
			std::shared_ptr<MessageCallReject> message = std::make_shared<MessageCallReject> ();
			message->code = MessageCallReject::Reason ::line_busy;
			conn->send(message->marshall());
			conn->close();
			return;
		}else{
			if( callin_denied_delay_ >0 ){ // 禁止呼入时段
				conn->userdata((void*)"reject");
				std::shared_ptr<MessageCallReject> message = std::make_shared<MessageCallReject> ();
				message->code = MessageCallReject::Reason ::call_denied;
				conn->send(message->marshall());
				conn->close();
				return;
			}
			//处理连接进入的呼叫
			std::shared_ptr<CallRequest> callin = std::make_shared<CallRequest>();
			callin->stime = std::time(NULL);
			conn->setListener(this);
			callin->conn = conn;
			callreq_ = callin;
			callreq_->direction = CallRequest::IN;
		}

	}else { // 处理向外呼叫 call outgoing
		// 呼叫连接建立，发送呼叫消息
		callreq_->conn->send( callreq_->msg->marshall());
		// 外呼 开启定时器，检测超时心跳包时候有效发送到达
//		deadline_timer_.expires_after(std::chrono::seconds(check_status_interval_));
//		deadline_timer_.async_wait(std::bind(&CallServiceManager::check_status, this));

	}
	live_time_outside_ = std::time(NULL);
	
	
}

//外部连接丢失
void CallServiceManager::onDisconnected(const Connection::Ptr& conn){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
	Application::instance()->getLogger().debug("Call Service Connection Lost.");

	if( conn->server() && conn->userdata() && ((char*) conn->userdata() ) == std::string("reject") ){
		//拒绝处理的连接
		return;
	}

	if(callreq_){
		callreq_.reset();
	}
//	deadline_timer_.cancel();	 // 撤销状态检查定时器
	InnerDeviceManager::instance()->endCall();
	live_time_inside_ = live_time_outside_ = 0;
}

void CallServiceManager::onData(const boost::asio::streambuf& buffer,const Connection::Ptr& conn){

}

//对外连接失败
void CallServiceManager::onConnectError(const Connection::Ptr& conn,ConnectionError error){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
	if(callreq_){
		callreq_->close();
		callreq_.reset();
	}
	InnerDeviceManager::instance()->rejectCall();
	live_time_inside_ = live_time_outside_ = 0;
	
}

void CallServiceManager::onJsonText(const std::string & text,const Connection::Ptr& conn){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
	Application::instance()->getLogger().debug("Outer Message Recved:" + text);
	Message::Ptr message = MessageJsonParser::parse(text.c_str(),text.size());
	if(!message){
		return ;
	}

	{
		std::shared_ptr<MessageCall> msg = std::dynamic_pointer_cast<MessageCall>(message);
		if (msg) {
			onCallIn(msg, conn);
			return;
		}
	}
	

	{ //远端呼叫接受
		std::shared_ptr<MessageCallAccept> msg = std::dynamic_pointer_cast<MessageCallAccept>(message);
		if(msg){
			onCallAccept(msg,conn);
			return;
		}
	}
	
	{ //远端呼叫拒绝
		std::shared_ptr<MessageCallReject> msg = std::dynamic_pointer_cast<MessageCallReject>(message);
		if(msg) {
			onCallReject(msg,conn);
			return;
		}
	}
	
	
	{ //呼叫停止
		std::shared_ptr<MessageCallEnd> msg = std::dynamic_pointer_cast<MessageCallEnd>(message);
		if(msg) {
			onCallEnd(msg,conn);
			return;
		}
	}
	
	{ //呼叫状态保持
		std::shared_ptr<MessageCallKeep> msg = std::dynamic_pointer_cast<MessageCallKeep>(message);
		if(msg) {
			onCallKeep(msg,conn);
			return;
		}
	}
	
}

//直接从室内app 透传发送消息到室外设备
void CallServiceManager::sendMessage(const std::shared_ptr<Message>  & msg) {
    std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
    if(callreq_){
        callreq_->conn->send(msg->marshall());
    }
}

void CallServiceManager::onCallKeep(const std::shared_ptr<MessageCallKeep>  & msg ,const Connection::Ptr& conn){
	//呼叫进入，内屏设备未接听时，外部设备连续发送call_keep,需将call_keep 连续发送给多个内部设备
	live_time_outside_ = std::time(NULL); // 保持活跃的对外呼叫连接
	auto retmsg = InnerDeviceManager::instance()->keepCall(msg->sid);

}

//外部返回呼叫拒绝
void CallServiceManager::onCallReject(const std::shared_ptr<MessageCallReject>  & msg ,const Connection::Ptr& conn){
	if(callreq_){
		callreq_->close();
		callreq_.reset();
	}
	InnerDeviceManager::instance()->rejectCall();
//	live_time_inside_ = live_time_outside_ = 0;
}

void CallServiceManager::onCallAccept(const std::shared_ptr<MessageCallAccept>  & msg ,const Connection::Ptr& conn){
	// 外呼时，被叫方拒绝，通知到室内屏设备，如果遭遇发送失败，则返回 CallEnd 消息
	auto retmsg = InnerDeviceManager::instance()->acceptCall();
	auto msgend= std::dynamic_pointer_cast<MessageCallEnd>(retmsg);
	if(msgend){
		if(callreq_){
			callreq_->conn->send(msgend->marshall()); // 告知远端呼叫终止
			callreq_->close();
			callreq_.reset();
		}
//		live_time_inside_ = live_time_outside_ = 0;
	}
}

void CallServiceManager::onCallIn(const std::shared_ptr<MessageCall> & msg,const Connection::Ptr& conn){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);

	callreq_->msg = msg;
	callreq_->open();
	std::shared_ptr<Message> message = InnerDeviceManager::instance()->callIn(callreq_);
	if(message){
		conn->send( message->marshall());
	}
}

void CallServiceManager::onCallEnd(const std::shared_ptr<MessageCallEnd> & msg,const Connection::Ptr& conn){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
	if(callreq_){ // 通知 InnerDeviceManager->
		InnerDeviceManager::instance()->endCall();
		callreq_->close();
		callreq_.reset();
//		live_time_inside_ = live_time_outside_ = 0;
	}else{
        InnerDeviceManager::instance()->endCall(); //未接听，主叫挂断
	}
	
}

// 呼入或呼出的请求结束 , 原因可以是主动挂断，或网络链接丢失
void CallServiceManager::endCall(){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
	if(callreq_){
		callreq_->conn->send(MessageCallEnd().marshall());
		callreq_->conn->close();
		callreq_.reset();
	}
}

std::shared_ptr<Message> CallServiceManager::keepCall(const std::string & sid){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
	live_time_inside_ = std::time(nullptr);
	if(callreq_){
		MessageCallKeep  keep;
		keep.sid = sid;
		callreq_->conn->send(keep.marshall());
		return std::shared_ptr<Message>();
	}
	// line of outside is lost.
	return std::make_shared<MessageCallEnd>();
}

void CallServiceManager::rejectCall(){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
	if(callreq_){
		callreq_->conn->send(MessageCallReject().marshall());
		callreq_->conn->close();
		callreq_.reset();
	}
}

std::shared_ptr<Message> CallServiceManager::acceptCall(){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
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

// 向外呼叫
bool CallServiceManager::callOut(const std::shared_ptr<MessageCall>& message){
	std::lock_guard<std::recursive_mutex> lock(this->rmutex_);
	Application::instance()->getLogger().debug("Do Call Out");

	if(callreq_){
		return false;
	}
	live_time_inside_ = std::time(NULL); //室内屏App请求呼叫开始,检测keepcall消息是否超时接收

	std::shared_ptr<CallRequest> callout = std::make_shared<CallRequest>();
	callout->src = message->src;
	callout->dest = message->dest;
	callout->stime = std::time(nullptr);
	callout->direction = CallRequest::Direction ::OUT;
	callout->msg = message;
	// 创建呼叫连接
	Connection::Ptr conn = std::make_shared<Connection>(server_->get_io_service());
	conn->setListener(this);
	
	callout->conn = conn;
	
	callreq_ = callout;

	// 室外机地址
//	auto ip  = InnerController::instance()->settings().outerbox;
	auto ip  = message->dest.ip;
	auto port = message->dest.port;
	Config& cfgs = Application::instance()->getConfig();
	if(message->dest.type == CallPeerType::INNER_SCREEN){
		// 户户对讲，推流地址定位到中心流服务器
		//这里室内机不用做处理
	}else{
	    if(0) { // 暂时关闭 2019.5.10 完全透传
            std::string url = cfgs.get_string("stream_url");
            std::string out_ip = InnerController::instance()->getOuterIP();
            std::string audio_url = (boost::format(url) % out_ip % message->src.audio_stream_id).str();
            std::string video_url = (boost::format(url) % out_ip % message->dest.video_stream_id).str();
            message->src.audio_stream_id = audio_url;
            message->src.video_stream_id = video_url;
        }
	}

//	auto ip = callout->dest.ip;
//	if(ip.length() == 0){ //未获得室外机地址
//		return  false;	// 直接告知室内屏 callreject
//	}
//	auto port = cfgs.get_int("outbox.port");
	boost::asio::ip::address address = boost::asio::ip::make_address(ip);
	boost::asio::ip::tcp::endpoint ep(address,port);
	conn->startConnect(ep);
	return true;
}


//设置 主机禁止呼入时间
void CallServiceManager::setCallInDenied(int delay){
	callin_denied_delay_ = delay;
}

int CallServiceManager::callinDenied(){
	return callin_denied_delay_;
}