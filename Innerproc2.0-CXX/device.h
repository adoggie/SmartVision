//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_DEVICE_H
#define INNERPROC_DEVICE_H

#include <memory>

/**
 * @brief 室内设备连接类型
 *
 */
class InnerDevice{
public:
	typedef std::shared_ptr<InnerDevice> Ptr;
	InnerDevice(const std::shared_ptr<Connection>& conn);

public:
	void open();
	void close();

	void sendData(BytePtr data ,size_t size);
	void setAccumulator(const std::shared_ptr<DataAccumulator>& acc);
	void setMessageHandler(const std::shared_ptr<MessageHandler> & handler);

	std::shared_ptr<Connection>& getConnection() const;
private:
	std::shared_ptr<Connection> _conn;
};





#endif //INNERPROC_DEVICE_H
