//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_SERVER_H
#define INNERPROC_SERVER_H

#include <string>
#include <memory>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "connection.h"
#include "service.h"


class ListenAddress{

};


//
//class NetworkServer{
//	void addListenService(const std::shared_ptr<ListenService> & service);
//};
//
//class SocketServer:NetworkServer{
//public:
//
//};
//
//class HttpServer:NetworkServer{
//public:
//	void addHandler(const std::shared_ptr<HttpHandler> & handler);
//};


//class InnerController:Service{
//
//public:
//
//};

class ListenServer{};


class SocketServer{
//	boost::asio::io_service& io_service;
//	boost::asio::ip::address _listen_address;
	boost::asio::ip::tcp::socket sock_;
	std::array<char,1024>   data_;
	std::string             ip_;
	unsigned short          port_;
//	std::function<void(Connection::Ptr)> conn_callback_;
	
	std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
//	std::shared_ptr<IConnectionListener> listener_;
	IConnectionListener* listener_;
public:
	SocketServer( boost::asio::io_service& io_service,const std::string& ip, const unsigned short port):sock_(io_service){
		acceptor_ = std::make_shared<boost::asio::ip::tcp::acceptor>(io_service);
		ip_ = ip ;
		port_ = port;
	}
	
//	boost::asio::ip::tcp::acceptor
	
	bool start();
//
//	void setListener(const std::shared_ptr<IConnectionListener>& listener){
//		listener_ = listener;
//	}
	
	void setListener(IConnectionListener* listener){
		listener_ = listener;
	}
	
//	std::shared_ptr<IConnectionListener> getListener(){
//		return listener_;
//	}
	
	IConnectionListener* getListener(){
		return listener_;
	}
	
	boost::asio::io_service & get_io_service(){
		return acceptor_->get_executor().context();
	}
	
	
	void close(){
	
	}
protected:
	void start_accept() {
		Connection::Ptr  new_connection ( new Connection(this));


		acceptor_->async_accept(new_connection->socket(),
		                       boost::bind(&SocketServer::handle_accept,
		                                                           this, new_connection,
		                                 boost::asio::placeholders::error));
	}
	
	void handle_accept(Connection::Ptr  new_connection, const boost::system::error_code& error) {
		if (!error) {
			if(listener_){
				new_connection->direction_ = Direction ::INCOMING;
				new_connection->setListener(listener_);
				listener_->onConnected(new_connection);
			}
			new_connection->start();
		}
		start_accept();
	}
	
protected:

};



#endif //INNERPROC_SERVER_H
