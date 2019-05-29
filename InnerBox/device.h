//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_DEVICE_H
#define INNERPROC_DEVICE_H

#include "base.h"
#include "connection.h"
#include "message.h"

/**
 * @brief 室内设备连接类型
 *
 */

 struct RegDeviceInfo{
 	std::time_t  time;
 	std::string  device_id;
 	std::string  device_type;
 	std::string  auth_code;
 };
 
class InnerDeviceManager;
class InnerDevice{
public:
	typedef std::shared_ptr<InnerDevice> Ptr;
	InnerDevice(const std::shared_ptr<Connection>& conn):conn_(conn){};
	InnerDevice();
public:
	bool open();
	void close();

//	void sendData(BytePtr data ,size_t size);
//	void setAccumulator(const std::shared_ptr<DataAccumulator>& acc);
//	void setMessageHandler(const std::shared_ptr<MessageHandler> & handler);

	std::shared_ptr<Connection>& connection() ;
	void connection(std::shared_ptr<Connection>& conn);
	friend class InnerDeviceManager;
	
	void sendMessage(const std::shared_ptr<Message>& msg );
private:
	std::shared_ptr<Connection> conn_;

	std::string token;
	std::string device_id;
	std::string device_type;
};


typedef std::map<std::string,InnerDevice::Ptr> InnerDeviceWithIds;




#endif //INNERPROC_DEVICE_H
