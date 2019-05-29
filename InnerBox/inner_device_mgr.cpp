//
// Created by bin zhang on 2019/1/10.
//
#include <mutex>

#include "base.h"
#include "inner_device_mgr.h"
#include "call_manager.h"
#include "message.h"
#include "app.h"

#include "InnerController.h"

#define SCOPE_LOCK std::lock_guard<std::recursive_mutex> lock(rmutex_);

bool  InnerDeviceManager::init(const Config& cfgs){
	cfgs_ = cfgs;
	std::string ip;
	unsigned  short port;
	ip = cfgs.get_string("family_ip","127.0.0.1");
	port = (unsigned short) cfgs.get_int("family_port",7892);

	ip = InnerController::instance()->getInnerIP();
	server_ = std::make_shared<SocketServer>(* InnerController::instance()->io_service(),ip,port);
	return true ;
}

bool InnerDeviceManager::open(){
	server_->setListener(this);
	server_->start();
	
	std::string ip;
	unsigned  short port;
	ip = cfgs_.get_string("family_ip","127.0.0.1");
	port = (unsigned short) cfgs_.get_int("family_port",7892);
	ip = InnerController::instance()->getInnerIP();

	Application::instance()->getLogger().info( (boost::format("InnerDeviceManager started.. serving on:%d,%s")%port%ip).str() );
	
	return true;
}

void InnerDeviceManager::close(){
	server_->close();
}

// APP 连接进入
void InnerDeviceManager::onConnected(const Connection::Ptr & conn){
	// 居然卡住了
//	std::lock_guard<std::recursive_mutex> lock(rmutex_);
	SCOPE_LOCK
	conn_ids_[conn->id()] = conn;
}

// 室内设备断开连接

void InnerDeviceManager::onDisconnected(const Connection::Ptr & conn){
//	std::lock_guard<std::mutex> lock(mutex_);
	SCOPE_LOCK
	Application::instance()->getLogger().debug("Inner Device Socket Lost!");
	{
		auto itr = conn_ids_.find(conn->id());
		if (itr != conn_ids_.end()) {
			conn_ids_.erase(itr);
		}
	}
	
	{
		auto itr = device_ids_.find(conn->id());
		if (itr != device_ids_.end()) {
			device_ids_.erase(itr);
		}
	}

	// 如果连接是通话连接，则需要告知通话远端
	if(calling_conn_ == conn){
		calling_conn_.reset();
		CallServiceManager::instance()->endCall();
	}

}

void InnerDeviceManager::onJsonText(const std::string & text,const Connection::Ptr& conn){
	//
//	std::lock_guard<std::mutex> lock(mutex_);
	SCOPE_LOCK
	Application::instance()->getLogger().debug("Inner Message Issued:" + text);
	Message::Ptr message = MessageJsonParser::parse(text.c_str(),text.length());
	if(!message){
		return ;
	}

	{//	请求加入家庭
		std::shared_ptr<MessageJoinFamily> msg = std::dynamic_pointer_cast<MessageJoinFamily>(message);
		if(msg) {
			onJoinFamily(msg,conn); // 设备加入家庭
		}
	}
	
	{ //对外呼叫
		std::shared_ptr<MessageCall> msg = std::dynamic_pointer_cast<MessageCall>(message);
		if(msg) onCallOut(msg,conn);
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

	{ //开门指令
		std::shared_ptr<MessageOpenDoor> msg = std::dynamic_pointer_cast<MessageOpenDoor>(message);
		if(msg){
			CallServiceManager::instance()->sendMessage(msg);
		}
	}


}

void InnerDeviceManager::onJoinFamily(const std::shared_ptr<MessageJoinFamily>& msg,const Connection::Ptr & conn){
	// verify msg->token
	// todo.  校验token有效性

	// 缓存设备对象，并发送同意加入消息
	InnerDevice::Ptr device = std::make_shared<InnerDevice>(conn);
	device->device_id = msg->id;
	device->device_type = msg->type;
	device->token = msg->token;
	device_ids_[conn->id()] = device; // 加入设备列表

	conn->send(MessageJoinAccept().marshall());
}

bool InnerDeviceManager::isBusy(){
	if(calling_conn_){
		return true;
	}
	return false;
}

//void InnerDeviceManager::callOut(CallInfo& call,std::shared_ptr<InnerDevice>& device){
//	std::lock_guard<std::recursive_mutex> lock(mutex_);
//}



void InnerDeviceManager::onCallOut(const std::shared_ptr<MessageCall>  & msg ,const Connection::Ptr& conn){
	//已有呼叫占线
	if(calling_conn_){
		conn->send(MessageCallReject(MessageCallReject::Reason::line_busy,"line_busy").marshall());
	}else{
		if(!CallServiceManager::instance()->callOut(msg)){ // call error
			conn->send(MessageCallReject(MessageCallReject::Reason::line_busy,"line_busy").marshall());
		}else {
			calling_conn_ = conn; // 设置当前设备的连接
		}
	}
	
}

//室内App发送 通话结束命令
void InnerDeviceManager::onCallEnd(const std::shared_ptr<MessageCallEnd>  & msg ,const Connection::Ptr& conn){
	if(!calling_conn_){ //当前未启动对话
	
	}else{
		if( calling_conn_ == conn) { //当前对话线路上发送的终止消息
			CallServiceManager::instance()->endCall();
			calling_conn_.reset();
		}
	}
}

//inner device send keep-message
void InnerDeviceManager::onCallKeep(const std::shared_ptr<MessageCallKeep>  & msg ,const Connection::Ptr& conn){
    Application::instance()->getLogger().debug("InnerMgr::OnCallKeep , redirect to Outer Accessor.");

    /*
	if( calling_conn_ != conn){
		conn->send(std::make_shared<MessageCallEnd>()->marshall());
		return;
	}
     */


	std::shared_ptr<Message> retmsg = CallServiceManager::instance()->keepCall(msg->sid);

	/*
	if(retmsg){
		conn->send(retmsg->marshall());
		{
			std::shared_ptr<MessageCallEnd> end = std::dynamic_pointer_cast<MessageCallEnd>(retmsg);
			if(end){ // reset current call_conn
				calling_conn_.reset();
			}
		}
		return;
	}

	if( calling_conn_ && calling_conn_ != conn){
		conn->send(std::make_shared<MessageCallEnd>()->marshall());
	}
    */
}

//任何一端室内App拒绝时，通知其他所有端会话呼叫会话结束
void InnerDeviceManager::onCallReject(const std::shared_ptr<MessageCallReject>  & msg ,const Connection::Ptr& conn){
	for(auto p: device_ids_){
		p.second->sendMessage(std::make_shared<MessageCallEnd>());
	}
	calling_conn_.reset();
	CallServiceManager::instance()->rejectCall();
}

// call established , notify other inner-device drop the call - request
void InnerDeviceManager::onCallAccept(const std::shared_ptr<MessageCallAccept>  & msg ,const Connection::Ptr& conn){
	//设备接听，通知其他室内设备呼叫停止
	for(auto p: device_ids_){
		if(p.second->connection() != conn){
			p.second->sendMessage(std::make_shared<MessageCallEnd>());
		}
	}

	Message::Ptr retmsg = CallServiceManager::instance()->acceptCall();
	if(retmsg){
		std::shared_ptr<MessageCallEnd> end = std::dynamic_pointer_cast<MessageCallEnd>(retmsg);
		if(end){ // outside connetion is lost  对外连接丢失，通知室内设备呼叫终止
			calling_conn_.reset();
			conn->send(std::make_shared<MessageCallEnd>()->marshall());
			return ;
		}
	}

	calling_conn_ = conn;

}

std::shared_ptr<Message> InnerDeviceManager::callIn(const std::shared_ptr<CallRequest>& req){
//	std::lock_guard<std::mutex> lock(mutex_);
	SCOPE_LOCK
	if(call_req_ || calling_conn_){ //等待呼入响应或已建立呼叫链路，则拒绝呼入
		return std::make_shared<MessageCallReject>(MessageCallReject::Reason::line_busy,"line_busy");
	}
	// 将呼叫请求广播给所有接入的设备
	if( device_ids_.size() == 0){
		return std::make_shared<MessageCallReject>(MessageCallReject::Reason::app_offline,"no app online");
	}
	for(auto p: device_ids_){
		p.second->sendMessage(req->msg);
	}
//	call_req_in_ = req;
	return std::shared_ptr<Message>();
}

// 停止对讲
void InnerDeviceManager::endCall(){
//	std::lock_guard<std::mutex> lock(mutex_);
	SCOPE_LOCK
	if(calling_conn_){
		calling_conn_->send( std::make_shared<MessageCallEnd>()->marshall());
		calling_conn_.reset();
	}else{ // 群发 挂断消息 ( 未接听时 ，通知所有室内设备挂断 )
		for (auto p: device_ids_) {
			p.second->sendMessage(std::make_shared<MessageCallEnd>());
		}
	}
}

// 呼叫未应答时也要连续发送 call_keep 消息到内屏设备
std::shared_ptr<Message> InnerDeviceManager::keepCall(const std::string & sid){

//	std::lock_guard<std::mutex> lock(mutex_);
	SCOPE_LOCK

    std::shared_ptr< MessageCallKeep> msg = std::make_shared<MessageCallKeep>();
    msg->sid = sid;
	if(calling_conn_){
		calling_conn_->send( msg->marshall());
	}else {
		for (auto p: device_ids_) {
			p.second->sendMessage(msg);
		}
	}
	
	return std::shared_ptr<Message>();
	
//	if(!calling_conn_) {  // connection of inside has lost , send call-end message to sender of MesageCallKeep
//		// 对话未开始，返回终止消息
//		return std::make_shared<MessageCallEnd>();
//	}
//
//	calling_conn_->send( std::make_shared<MessageCallKeep>()->marshall());
//	return std::shared_ptr<Message>();
}

void InnerDeviceManager::rejectCall(){
//	std::lock_guard<std::mutex> lock(mutex_);
	SCOPE_LOCK
	if(calling_conn_){
		calling_conn_->send( std::make_shared<MessageCallReject>()->marshall());
		calling_conn_.reset();
	}
}

std::shared_ptr<Message> InnerDeviceManager::acceptCall(){
//	std::lock_guard<std::mutex> lock(mutex_);
	SCOPE_LOCK
	if(!calling_conn_) {  // connection of inside has lost , send call-end message to sender of MesageCallKeep
		return std::make_shared<MessageCallEnd>();
	}
	calling_conn_->send( std::make_shared<MessageCallAccept>()->marshall());
	return std::shared_ptr<Message>();
}


//广播报警消息
void InnerDeviceManager::postEmergency(const std::shared_ptr<MessageEmergency> & message){
//	std::lock_guard<std::mutex> lock(mutex_);
	SCOPE_LOCK
	// 将呼叫请求广播给所有接入的设备
	for(auto p: device_ids_){
		Application::instance()->getLogger().debug("emergency broadcast: port="+ boost::lexical_cast<std::string>(message->port) + \
		" device_session_id:"+p.first
		);
		p.second->sendMessage(message);
	}
}


/**
 *
 * @param message
 * @return
 *   false - 室内设备的呼叫撤销
 */
//bool InnerDeviceManager::sendMessageOnCalling(const std::shared_ptr<Message>& message){
//	std::lock_guard<std::mutex> lock(mutex_);
//	if(calling_conn_){
//		calling_conn_->send(message->marshall());	// 告知通话中的设备通话终止
//		{
//			std::shared_ptr<MessageCallEnd> msg = std::dynamic_pointer_cast<MessageCallEnd>(message);
//			if(msg){
//				calling_conn_.reset();
//				return true;
//			}
//		}
//		{
//			std::shared_ptr<MessageCallReject> msg = std::dynamic_pointer_cast<MessageCallReject>(message);
//			if(msg){
//				calling_conn_.reset();
//				return true;
//			}
//		}
//		return true;
//	}
//	return false;
//}