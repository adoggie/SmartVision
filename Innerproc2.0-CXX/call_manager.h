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
class CallServiceManager:Service,IConnectionListener,std::enable_shared_from_this<CallServiceManager>{
	
	std::shared_ptr<SocketServer> server_;  /*!< 接收呼叫进入的链接管理服务器 */
	std::shared_ptr<CallRequest> callreq_;  /*!< 呼入或呼出 请求 */
	
//	struct Listener:IConnectionListener{
//
//	};
	
	std::shared_ptr<Listener> listener_;
public:
	
	static std::shared_ptr<CallServiceManager>& instance(){
		static std::shared_ptr<CallServiceManager> handle ;
		if(!handle.get()){
			handle = new CallServiceManager;
		}
	}
	
	
	bool init(const Config& cfg);
	void open();
	void close();
	void run();
	
	void onConnected(Connection::Ptr& conn);
	void onDisconnected(Connection::Ptr& conn);
	void onData(boost::asio::streambuf& buffer);
	void onJsonText(const std::string & text);
	void onCallIn(const std::shared_ptr<MessageCallIn> & msg,Connection::Ptr& conn);
	
	void onCallEnd(std::shared_ptr<CallRequest>& callreq);
	// 呼入或呼出的请求结束 , 原因可以是主动挂断，或网络链接丢失
	void endCall();     // 结束呼叫   (app 发送挂断)
	void keepCall();    // 保持呼叫线路 （由app定时发送)
	void rejectCall();  // 拒接
	void acceptCall();  // 接听
	bool callOut(std::shared_ptr<CallInfo>& info);  // 呼叫对方
	void sendOnlineMessage(const Message::Ptr& msg); // 直接往呼叫通道上发送消息
};

#endif //INNERPROC_CALL_MANAGER_H
