#include "server.h"
#include "app.h"

bool SocketServer::start(){
	boost::asio::ip::address address = boost::asio::ip::make_address(ip_);
	boost::asio::ip::tcp::endpoint ep(address,port_);
	acceptor_->open(ep.protocol());
	acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_->bind(ep);
	acceptor_->listen();
	
	Application::instance()->getLogger().info( (boost::format("SocketServer Bind (%s:%d)")%ip_%port_).str() );

//		Connection::Ptr new_connection = std::make_shared<Connection>();
//		acceptor_->async_accept(new_connection->socket(),
//		                       boost::bind(&SocketServer::handle_accept, this, new_connection,
//		                                   boost::asio::placeholders::error));
	start_accept();
	return true;
}