//
// Created by bin zhang on 2019/1/10.
//

#ifndef INNERPROC_INNER_DEVICE_MGR_H
#define INNERPROC_INNER_DEVICE_MGR_H

#include <memory>

#include "device.h"
#include "service.h"
#include "sensor.h"
#include "server.h"
#include "message.h"

/**
 * @brief 室内设备连接管理器
 */
class InnerDeviceManager: Service,IConnectionListener { // ,std::enable_shared_from_this<InnerConnectionManager>{

public:
	static std::shared_ptr<InnerDeviceManager>& instance(){
		static std::shared_ptr<InnerDeviceManager> handle ;
		if(!handle.get()){
			handle = std::make_shared<InnerDeviceManager>() ;
		}
		return handle;
	}
	
	bool  init(const Config& cfgs);
	
	bool open();
	void close();
	
//	void onCallComeIn(const CallInfo& callinfo,std::shared_ptr<Connection>& conn);
	
//	void onMessage(const std::shared_ptr<Message>& message,const std::share_ptr<InnerDevice>& device);
	void onJoinFamily(const std::shared_ptr<MessageJoinFamily> & msg,const Connection::Ptr& conn);
	void onCallOut(const std::shared_ptr<MessageCallOut>  & msg ,const Connection::Ptr& conn);
	void onCallEnd(const std::shared_ptr<MessageCallEnd>  & msg ,const Connection::Ptr& conn);
	void onCallKeep(const std::shared_ptr<MessageCallKeep>  & msg ,const Connection::Ptr& conn);
	void onCallReject(const std::shared_ptr<MessageCallReject>  & msg ,const Connection::Ptr& conn);
	void onCallAccept(const std::shared_ptr<MessageCallAccept>  & msg ,const Connection::Ptr& conn);
	
	
	void onConnected(const Connection::Ptr & conn);
	void onDisconnected(const Connection::Ptr & conn);
	void onJsonText(const std::string & text,const Connection::Ptr& conn);
	
	bool isBusy();
	void postAlarm(const std::shared_ptr<SensorAlarmInfo> & alarm);     /*!< 广播报警到连接的室内设备 */
	std::shared_ptr<Message> callIn(const std::shared_ptr<CallRequestIn>& req);     /*!< 呼叫进入 */
	
	void endCall();     // 远端结束呼叫   (app 发送挂断)
	std::shared_ptr<Message> keepCall();    // 远端保持呼叫线路 （由app定时发送)
	void rejectCall();  // 远端拒接
	std::shared_ptr<Message> acceptCall();  // 远端接听
	
	bool sendMessageOnCalling(const std::shared_ptr<Message>& message);  // 在呼叫设备连接上发送消息
protected:
//	void callOut(CallInfo& call,std::shared_ptr<InnerDevice>& device);	/*!<发起对外呼叫 */
protected:
//	void onJoinFamily(std::shared_ptr<MessageJoinFamily>& msg,const Connection::Ptr & conn);
	
	std::list< InnerDevice::Ptr >   devices_;
	std::shared_ptr<SocketServer>   sockserver_;
	
	Connection::Ptr  calling_conn_; //  当前呼叫中的连接
	InnerDeviceWithIds device_ids_; //
	std::map<std::string,Connection::Ptr> conn_ids_;
	std::shared_ptr<SocketServer>    server_;
	boost::asio::io_service io_service_;
	std::shared_ptr<CallRequestIn>  call_req_in_; //呼叫进入，accept/reject/timeout将清除此对象
};




#endif //INNERPROC_INNER_DEVICE_MGR_H
