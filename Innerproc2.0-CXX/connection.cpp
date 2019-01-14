//
// Created by bin zhang on 2019/1/10.
//
#include "connection.h"
#include "server.h"
#include <iostream>
#include <ctime>


//Connection::Connection(){
//	id_ = make_id();
//	server_ = nullptr;
//
//}


Connection::Connection( boost::asio::io_service& io_service):
		sock_(io_service),deadline_timer_(io_service),heartbeat_timer_(io_service){

}

Connection::Connection(SocketServer* server):
		sock_(server->get_io_service()),
		deadline_timer_(server->get_io_service()),
		heartbeat_timer_(server->get_io_service()),
		server_(server){
	direction_ = Direction::OUTGOING;
}


std::string Connection::make_id(){
	std::time_t result = std::time(nullptr);
	return std::asctime(std::localtime(&result));
}

void Connection::handle_read(const boost::system::error_code& error, size_t bytes_transferred){
	const char SP ='\0';
	if (stopped_) {
		return;
	}
	
	if (!error) {
		std::cout << bytes_transferred << " bytes have read " << "m_streambuf.size()=" << streambuf_.size() << std::endl;
		
		std::vector<std::string> json_lines;
		const char * data = (const char*)streambuf_.data().data();
		size_t size = streambuf_.data().size();
		
		bool keep_last = false;
		if( data[size-1]!=SP){
			keep_last = true;
		}
		
		std::string text;
		std::istream is(&streambuf_);
		while(streambuf_.size()){
			std::getline(is,text,SP);
			json_lines.push_back(text);
		}
		
		if(keep_last) {
			text = json_lines[json_lines.size()-1];
			json_lines.pop_back();
			
			std::ostream os(&streambuf_);
			os << text;
		}
		
		// parse json message
		for(auto s : json_lines) {
			server_->getListener()->onJsonText(s,shared_from_this());
		}

	}else {
		std::cerr << error.message() << std::endl;
		close();
	}
	
}

void Connection::open(){
//	boost::asio::async_read_until(sock_,streambuf_,"\0",
//	                              std::bind(&Connection::handle_read, shared_from_this(),
//	                                        boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
//
//

}


void Connection::send(const char * data,size_t size){
	boost::asio::async_write(sock_, boost::asio::buffer(data,size), boost::bind(&Connection::handle_write, shared_from_this(),
	                                                                            boost::asio::placeholders::error,
	                                                                          boost::asio::placeholders::bytes_transferred));
}

void Connection::send(const std::string& data){
	send(data.c_str(),data.size());
}

void Connection::startConnect(const boost::asio::ip::tcp::endpoint& ep ){
	ep_ = ep;
//		conn_timeout_ = timeout_;
	connect();
}

void Connection::connect(){
	deadline_timer_.expires_after(std::chrono::seconds(conn_timeout_));
	deadline_timer_.async_wait(std::bind(&Connection::check_connect, this));
	sock_.async_connect(ep_,
	                      boost::bind(&Connection::handle_connect,
	                                this, _1));
}


void Connection::close(){
	stopped_ = true;
	boost::system::error_code ignored_error;
	sock_.close(ignored_error);
	deadline_timer_.cancel();
	heartbeat_timer_.cancel();
}



void Connection::start_read() {
	// Set a deadline for the read operation.
	if(read_timeout_) {
		deadline_timer_.expires_after(std::chrono::seconds(read_timeout_));
	}
	
	// Start an asynchronous operation to read a newline-delimited message.
//		boost::asio::async_read_until(socket_,
//		                              boost::asio::dynamic_buffer(input_buffer_), '\n',
//		                              std::bind(&client::handle_read, this, _1, _2));
	
	boost::asio::async_read_until(sock_,streambuf_,"\0",
	                              boost::bind(&Connection::handle_read, shared_from_this(),
	                                        boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	
}


void Connection::start_heartbeat() {
	if (stopped_)
		return;
	
	// Start an asynchronous operation to send a heartbeat message.
	heartbeat_timer_.expires_after(std::chrono::seconds(read_timeout_));
	heartbeat_timer_.async_wait(std::bind(&Connection::start_heartbeat, this));
	send(MessageHeartbeat().marshall());
	
}


void Connection::check_connect(){
	if(stopped_){
		return;
	}
	if(deadline_timer_.expiry() <= boost::asio::steady_timer::clock_type::now()){
		sock_.close() ; // 超时了关闭socket
		deadline_timer_.expires_at(boost::asio::steady_timer::time_point::max()); // 设置永不触发
	}
	deadline_timer_.async_wait(std::bind(&Connection::check_connect, this));
}



void Connection::handle_write(const boost::system::error_code& error, size_t bytes_transferred) {
	if (stopped_) {
		return;
	}
	
	if (!error) {
//			heartbeat_timer_.expires_after(std::chrono::seconds(read_timeout_));
//			heartbeat_timer_.async_wait(std::bind(&Connection::start_write, this));
	}
	else
	{
		std::cout << "Error on heartbeat: " << error.message() << "\n";
		close();
	}
}


void Connection::handle_connect(const boost::system::error_code& error) {
	if (stopped_)
		return;
	
	// The async_connect() function automatically opens the socket at the start
	// of the asynchronous operation. If the socket is closed at this time then
	// the timeout handler must have run first.
	if (!sock_.is_open()) {
		std::cout << "Connect timed out\n";
		if(listener_){
			listener_->onConnectError(shared_from_this(),ConnectionError::Timeout);
		}
	}
		
		// Check if the connect operation failed before the deadline expired.
	else if (error)
	{
		std::cout << "Connect error: " << error.message() << "\n";
		
		// We need to close the socket used in the previous connection attempt
		// before starting a new one.
		sock_.close();
		if(listener_){
			listener_->onConnectError(shared_from_this(),ConnectionError::Unreachable);
		}
		
	}
		// Otherwise we have successfully established a connection.
	else {
//		std::cout << "Connected to " << endpoint_iter->endpoint() << "\n";
		if(listener_){
			listener_->onConnected(shared_from_this());
		}
		// Start the input actor.
		start_read();
		// Start the heartbeat actor.
		start_heartbeat();
	}
}

//////

//
//void WithPeerConnection::keep_alive(std::int32_t  interval){
//
//}
//
//void WithPeerConnection::open(){
//
//}
//
//void WithPeerConnection::close(){
//
//}


////与物业中心链接
//class PropertyServerConnection:WithPeerConnection{
//
//};
//
////与室外机链接
//class OuterBoxConnection:WithPeerConnection{
//
//};
