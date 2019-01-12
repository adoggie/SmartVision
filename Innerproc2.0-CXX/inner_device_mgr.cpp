//
// Created by bin zhang on 2019/1/10.
//

#include "inner_device_mgr.h"
#include "call_manager.h"


bool  InnerDeviceManager::init(const Config& cfgs){
	cfgs_ = cfgs;
	return true ;
}

void InnerDeviceManager::open(){

}

void InnerDeviceManager::close(){

}

//void InnerDeviceManager::onCallComeIn(const CallInfo& callinfo,std::shared_ptr<Connection>& conn){
//
//}

//void InnerDeviceManager::onMessage(const std::shared_ptr<Message>& message,const std::share_ptr<InnerDevice>& device){
//
//}

void InnerDeviceManager::onConnected(const std::shared_ptr<Connection> & conn){

}

void InnerDeviceManager::onDisconnected(const std::shared_ptr<Connection> & conn){
	std::lock_guard<std::mutex> lock(mutex_);
	
	tgObject::Ptr data = conn->data();
	if(data){
	}
	if(calling_conn_) {
		if (conn == calling_conn_) {
			CallServiceManager.instance().endCall();
		}
		calling_conn_.reset();
	}
}

bool InnerDeviceManager::isBusy(){
	if(calling_conn_){
		return true;
	}
	return false;
}

void InnerDeviceManager::callOut(CallInfo& call,std::shared_ptr<InnerDevice>& device){

}


void InnerDeviceManager::onJoinFamily(const std::shared_ptr<MessageJoinFamily> & msg,Connection::Ptr& conn){

}

void InnerDeviceManager::onCallOut(const std::shared_ptr<MessageCallOut>  & msg ,Connection::Ptr& conn){

}

void InnerDeviceManager::onCallEnd(const std::shared_ptr<MessageCallEnd>  & msg ,Connection::Ptr& conn){

}

void InnerDeviceManager::onCallKeep(const std::shared_ptr<MessageCallKeep>  & msg ,Connection::Ptr& conn){

}

void InnerDeviceManager::onCallReject(const std::shared_ptr<MessageCallReject>  & msg ,Connection::Ptr& conn){

}

void InnerDeviceManager::onCallAccept(const std::shared_ptr<MessageCallAccept>  & msg ,Connection::Ptr& conn){

}

void InnerDeviceManager::postAlarm(const std::shared_ptr<SensorAlarmInfo> & alarm){

}

bool InnerDeviceManager::callIn(const std::shared_ptr<CallRequestIn>& req){
	return false;
}

// 停止对讲
void InnerDeviceManager::endCall(){
	std::lock_guard<std::mutex> lock(mutex_);
	if(calling_conn_){
		calling_conn_->send(MessageCallEnd().marshall());
		calling_conn_->close();
		calling_conn_.reset();
	}
}

void InnerDeviceManager::keepCall(){

}

void InnerDeviceManager::rejectCall(){

}

void InnerDeviceManager::acceptCall(){

}
