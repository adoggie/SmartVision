#include "device.h"


InnerDevice::InnerDevice(){

}

bool InnerDevice::open(){
	return true;
}

void InnerDevice::close(){

}

//void InnerDevice::sendData(BytePtr data ,size_t size){
//
//}

//void setAccumulator(const std::shared_ptr<DataAccumulator>& acc);
//	void setMessageHandler(const std::shared_ptr<MessageHandler> & handler);

std::shared_ptr<Connection>& InnerDevice::connection() {
	return conn_;
}

void InnerDevice::connection(std::shared_ptr<Connection>& conn){
	conn_ = conn;
}

void InnerDevice::sendMessage(const std::shared_ptr<Message>& msg ){
	conn_->send(msg->marshall());
}