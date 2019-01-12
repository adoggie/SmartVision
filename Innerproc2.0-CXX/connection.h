//
// Created by bin zhang on 2019/1/6.
//

#ifndef BRANCHES_CONNECTION_H
#define BRANCHES_CONNECTION_H

#include "base.h"
#include "connection.h"

#include <memory>
#include <mutex>
#include <list>
#include <boost/asio.hpp>

#include "codec.h"

enum class ConnectionType{
	SOCKET,
	INNER_DEVICE,

};


class SocketServer;

enum Direction{
	INCOMING,
	OUTGOING
};

struct IConnectionListener;
/**
 * @brief 网络链接基类
 */
class  Connection: tgObject, std::enable_shared_from_this<Connection>{
	
	boost::asio::ip::tcp::socket sock_;	/*!< socket 句柄 */
	std::string ip_;
	unsigned  short port_;
	std::shared_ptr< DataConsumer > consumer_;
	SocketServer* server_;
	Direction  direction_;
	boost::asio::streambuf streambuf_;
	std::shared_ptr<IConnectionListener> listener_;
	std::string id_;

protected:
	std::string make_id();
public:
	typedef std::shared_ptr<Connection> Ptr;
	
	friend class  SocketServer;
	
	Connection();
	Connection(const boost::asio::io_service& io_service);
	Connection(SocketServer* server);
	void setListener(const std::shared_ptr<IConnectionListener> listener){
		listener_ = listener;
	}
	
	std::string id(){ return id_;}
	boost::asio::ip::tcp::socket& socket(){
		return sock_;
	}
	
	void close(){
	
	}
	
	void send(const char * data,size_t size){
		boost::asio::async_write(sock_, boost::asio::buffer(data,size), std::bind(&Connection::handle_write, shared_from_this(),
		                                                                                boost::asio::placeholders::error,
		                                                                               boost::asio::placeholders::bytes_transferred));
	}
	
	void send(const std::string& data){
	
	}
	
	
	void open();
protected:
	void handle_write(const boost::system::error_code& error, size_t bytes_transferred) {
	
	}
	
	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
};


struct IConnectionListener{
//	virtual void onConnectionReached(Connection::Ptr& conn) = 0;
	virtual void onConnected(Connection::Ptr& conn)=0;
	virtual void onDisconnected(Connection::Ptr& conn){};
	virtual void onData(boost::asio::streambuf& buffer){};
	virtual void onJsonText(const std::string & text,Connection::Ptr& conn){};
	
};


//对点服务器链接
class WithPeerConnection: Connection { //IConnectionListener,std::enable_shared_from_this<WithPeerConnection>{
protected:
//	Connection::Ptr conn_;
	std::int32_t     interval_; // 心跳间隔
	boost::asio::ip::tcp::endpoint endpoint_;
protected:
	virtual ~WithPeerConnection(){}
	
public:
	WithPeerConnection(const boost::asio::ip::tcp::endpoint& ep):endpoint_(ep){
	
	}
	
	void setEndpoint(const boost::asio::ip::tcp::endpoint& ep){
		endpoint_ = ep;
	}
	
	void keep_alive(std::int32_t  interval);
	void open();
	void close();
};

//与物业中心链接
class PropertyServerConnection:WithPeerConnection{

public:
	PropertyServerConnection(){}
};

//与室外机链接
class OuterBoxConnection:WithPeerConnection{
public:
	OuterBoxConnection(){}
};


#endif //BRANCHES_CONNECTION_H
