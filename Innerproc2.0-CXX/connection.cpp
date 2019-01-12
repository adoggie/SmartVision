//
// Created by bin zhang on 2019/1/10.
//
#include "connection.h"
#include "server.h"
#include <iostream>
#include <ctime>


Connection::Connection(){
	id_ = make_id();
	server_ = nullptr;
}


Connection::Connection(const boost::asio::io_service& io_service),Connection(){

}

Connection::Connection(SocketServer* server): Connection(),sock_(server->get_io_service()),server_(server){
	direction_ = Direction::OUTGOING;
}


std::string Connection::make_id(){
	std::time_t result = std::time(nullptr);
	return std::asctime(std::localtime(&result));
}

void Connection::handle_read(const boost::system::error_code& error, size_t bytes_transferred){
	const char SP ='\0';
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
			server_->get_listener()->onJsonText(s);
		}

//			size_t i = 0;
//			streambuf_.commit(bytes_transferred);
//
//			stringstream ss;
//			ss << &m_streambuf;
//			std::string word = ss.str();
//			const size_t szWord = word.size();
//			cout << "word.size()=" << szWord << endl;
//			word.resize(bytes_transferred);
//			const size_t szWord2 = word.size();
//			cout << word << endl;
//			for (auto it = word.begin(); i < szWord2; it++, i++) {
//				if (*it >= 33 && *it <= 126) {
//					cout << *it << " ";
//				}else {
//					cout << '\\' << int(*it) << " ";
//				}
//			}
//			cout << endl;
//			cout << &m_streambuf << endl;
//			m_streambuf.consume(m_streambuf.size());
//			boost::asio::async_read_until(socket_, m_streambuf, m_reg, boost::bind(&tcp_connection::handle_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}else {
		std::cerr << error.message() << std::endl;
	}
	
}

void Connection::open(){
	boost::asio::async_read_until(sock_,streambuf_,"\0",
	                              std::bind(&Connection::handle_read, shared_from_this(),
	                                        boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)

//		boost::asio::async_write(_sock, boost::asio::buffer("first message"), std::bind(&Connection::handle_write, shared_from_this(),
//		                                                                                boost::asio::placeholders::error,
//		                                                                               boost::asio::placeholders::bytes_transferred));
//

//		boost::asio::async_
//		_sock.async_read_some()
//				_sock.async_receive()
//
//						_sock.async_send()


}


//////


void WithPeerConnection::keep_alive(std::int32_t  interval){

}

void WithPeerConnection::open(){

}

void WithPeerConnection::close(){

}


//与物业中心链接
class PropertyServerConnection:WithPeerConnection{

};

//与室外机链接
class OuterBoxConnection:WithPeerConnection{

};
