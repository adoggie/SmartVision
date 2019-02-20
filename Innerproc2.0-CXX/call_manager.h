//
// Created by bin zhang on 2019/1/10.
//

#ifndef INNERPROC_CALL_MANAGER_H
#define INNERPROC_CALL_MANAGER_H

#include <memory>

#include "call.h"
#include "service.h"
#include "server.h"
#include "message.h"

/**
 * @brief 呼叫服务管理器
 * 负责呼叫进入和外出呼叫的管理功能
 */
class CallServiceManager:public Service,IConnectionListener, public std::enable_shared_from_this<CallServiceManager>{

public:
	
	static std::shared_ptr<CallServiceManager>& instance(){
		static std::shared_ptr<CallServiceManager> handle ;
		if(!handle.get()){
			handle = std::make_shared<CallServiceManager>() ;
		}
		return handle;
	}
	
	CallServiceManager();
	
	bool init(const Config& cfg);
	bool open();
	void close();
	void run();
	
	void onConnected(const Connection::Ptr& conn);
	void onDisconnected(const Connection::Ptr& conn);
	void onData(const boost::asio::streambuf& buffer,const Connection::Ptr& conn);
	void onJsonText(const std::string & text,const Connection::Ptr& conn);
	void onConnectError(const Connection::Ptr& conn,ConnectionError error);
	
	void onCallIn(const std::shared_ptr<MessageCallIn> & msg,const Connection::Ptr& conn);
	void onCallEnd(const std::shared_ptr<MessageCallEnd> & msg,const Connection::Ptr& conn);
	void onCallKeep(const std::shared_ptr<MessageCallKeep>  & msg ,const Connection::Ptr& conn);
	void onCallReject(const std::shared_ptr<MessageCallReject>  & msg ,const Connection::Ptr& conn);
	void onCallAccept(const std::shared_ptr<MessageCallAccept>  & msg ,const Connection::Ptr& conn);
	
	
	// 呼入或呼出的请求结束 , 原因可以是主动挂断，或网络链接丢失
	void endCall();     // 结束呼叫   (app 发送挂断)
	std::shared_ptr<Message> keepCall();    // 保持呼叫线路 （由app定时发送)
	void rejectCall();  // 拒接
	std::shared_ptr<Message> acceptCall();  // 接听
	bool callOut(const CallInfo& info);  // 呼叫对方
//	void sendMessageOnCalling(const Message::Ptr& msg); // 直接往呼叫通道上发送消息

	void setCallInDenied(int delay);	//不允许其它业主呼入，设置室内机不接受其它业主的呼叫， 0 ： 取消 ， 单位: 分钟
	int callinDenied();			// 返回禁止呼入的时间
private:
	void check_status();
//	void thread_run( );
private:
	
	std::shared_ptr<SocketServer> server_;  /*!< 接收呼叫进入的链接管理服务器 */
	std::shared_ptr<CallRequest> callreq_;  /*!< 呼入或呼出 请求 */
	
	std::size_t  check_status_interval_ = 5; //
	std::time_t  live_time_inside_ = 0;		//室内机发送消息时间
	std::time_t  live_time_outside_ =0 ;	//外部设备进入消息时间
	boost::asio::steady_timer deadline_timer_;
//	std::shared_ptr<std::thread> thread_;

	std::atomic_int  	callin_denied_delay_ ;
};

#endif //INNERPROC_CALL_MANAGER_H
