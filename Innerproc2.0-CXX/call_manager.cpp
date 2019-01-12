//
// Created by bin zhang on 2019/1/10.
//

#include "call_manager.h"
#include <jsoncpp/json/json.h>
#include "message.h"


bool CallServiceManager::init(const Config& cfg){
	server_->setListener(std::shared_from_this());
	return true;
}

void CallServiceManager::open(){

}

void CallServiceManager::close(){

}

void CallServiceManager::run(){

}

void CallServiceManager::onConnected(Connection::Ptr& conn){

}

void CallServiceManager::onDisconnected(Connection::Ptr& conn){

}

void CallServiceManager::onData(boost::asio::streambuf& buffer){

}

void CallServiceManager::onJsonText(const std::string & text,Connection::Ptr& conn){
//	Json::Reader reader;
//	Json::Value root;
//	if (reader.parse(str, root))  // reader将Json字符串解析到root，root将包含Json里所有子元素
//	{
//		std::string upload_id = root["uploadid"].asString();  // 访问节点，upload_id = "UP000000"
//		int code = root["code"].asInt();    // 访问节点，code = 100
//	}
	Message::Ptr msg = MessageJsonParser::parse(text.c_str(),text.size());
	if(!msg){
		return ;
	}
	
	std::shared_ptr<MessageCallIn> msg_call = std::dynamic_pointer_cast<MessageCallIn>(msg);
	if(msg_call){
		onCallIn(msg_call,conn);
	}
}

void CallServiceManager::onCallIn(const std::shared_ptr<MessageCallIn> & msg,Connection::Ptr& conn){
	std::lock_guard<std::mutex> lock(this->mutex_);
	
	if(callreq_){
		// return LineBusy
		conn->send(MessageLineBusy().marshall());
		return ;
	}
	std::shared_ptr<CallRequestIn> reqin = std::make_shared<CallRequestIn>(msg);
	reqin->conn = conn;
	callreq_ = reqin;
	callreq_->open();
}

void CallServiceManager::onCallEnd(std::shared_ptr<CallRequest>& callreq){
	std::lock_guard<std::mutex> lock(this->mutex_);
	if(callreq_){ // 通知 InnerDeviceManager->
	
	}
	callreq_.reset();
}

// 呼入或呼出的请求结束 , 原因可以是主动挂断，或网络链接丢失
void CallServiceManager::endCall(){

}

void CallServiceManager::keepCall(){

}

void CallServiceManager::rejectCall(){

}

void CallServiceManager::acceptCall(){

}

void CallServiceManager::sendCallMessage(const Message::Ptr& msg){

}
