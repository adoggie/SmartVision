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
#include <boost/bind.hpp>

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

enum class ConnectionError{
	Unreachable,
	Timeout
};

struct IConnectionListener;
/**
 * @brief 网络链接基类
 */
//class  Connection: tgObject, std::enable_shared_from_this<Connection>{
class  Connection: public std::enable_shared_from_this<Connection>{
//class  Connection{
	std::mutex mutex_;
	boost::asio::ip::tcp::socket sock_;	/*!< socket 句柄 */
//	std::string ip_;
//	unsigned  short port_;
	std::shared_ptr< DataConsumer > consumer_;
	SocketServer* server_;
	Direction  direction_;
	boost::asio::streambuf streambuf_;
//	std::shared_ptr<IConnectionListener> listener_;
	IConnectionListener* listener_;
	std::string id_;
	boost::asio::steady_timer deadline_timer_;
	boost::asio::steady_timer heartbeat_timer_;
	std::size_t  conn_timeout_ = 30 ;    // 连接超时
	std::size_t  read_timeout_ = 0 ;    // 读超时
	std::size_t  heartbeat_=0;          // 心跳
	bool stopped_ = false;
	boost::asio::ip::tcp::endpoint ep_;
protected:
	std::string make_id();
public:
	typedef std::shared_ptr<Connection> Ptr;
	
	friend class  SocketServer;
	
//	Connection();
	Connection( boost::asio::io_service& io_service);
//	Connection(const boost::asio::ip::tcp::endpoint& ep,const boost::asio::io_service& io_service);
	Connection(SocketServer* server);
	virtual  ~Connection(){}
	void setListener(IConnectionListener* listener){
		listener_ = listener;
	}
	
	std::string id(){ return id_;}
	boost::asio::ip::tcp::socket& socket(){
		return sock_;
	}
	
	Connection& heartBeatTime(std::size_t  hbt){
		heartbeat_ = hbt;
		return *this;
	}
	
	Connection& connectTimeout(std::size_t  timeout){
		conn_timeout_ = timeout;
		return *this;
	}
	
	Connection& readTimeout(std::size_t  timeout){
		read_timeout_ = timeout;
		return *this;
	}
	
	void send(const char * data,size_t size);
	
	void send(const std::string& data);
	
	void startConnect(const boost::asio::ip::tcp::endpoint& ep );
	
	void connect();
	void open();
	void close();
	
	void start_read() ;
	
	void start_heartbeat() ;
	void start();
protected:
	
	void check_connect();
	
	void handle_write(const boost::system::error_code& error, size_t bytes_transferred) ;
	
	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
	
	void handle_connect(const boost::system::error_code& error) ;
	
};


struct IConnectionListener{
//	virtual void onConnectionReached(Connection::Ptr& conn) = 0;
	virtual void onConnected(const Connection::Ptr& conn){};
	virtual void onDisconnected(const Connection::Ptr& conn){};
	virtual void onData(boost::asio::streambuf& buffer,const Connection::Ptr& conn){};
	virtual void onJsonText(const std::string & text,const Connection::Ptr& conn){};
	virtual void onConnectError(const Connection::Ptr& conn,ConnectionError error){}  // 0: not reachable , 1: timeout
	
};


////对点服务器链接
//class WithPeerConnection: Connection { //IConnectionListener,std::enable_shared_from_this<WithPeerConnection>{
//protected:
////	Connection::Ptr conn_;
//	std::int32_t     interval_; // 心跳间隔
//	boost::asio::ip::tcp::endpoint endpoint_;
//protected:
//	virtual ~WithPeerConnection(){}
//
//public:
//	WithPeerConnection(){}
//	WithPeerConnection(const boost::asio::ip::tcp::endpoint& ep):endpoint_(ep){
//
//	}
//
//	void setEndpoint(const boost::asio::ip::tcp::endpoint& ep){
//		endpoint_ = ep;
//	}
//
//	void keep_alive(std::int32_t  interval);
//	void open();
//	void close();
//};
//
////与物业中心链接
//class PropertyServerConnection:WithPeerConnection{
//
//public:
//	PropertyServerConnection(){}
//};
//
////与室外机链接
//class OuterBoxConnection:WithPeerConnection{
//public:
//	OuterBoxConnection(){}
//};


#endif //BRANCHES_CONNECTION_H
