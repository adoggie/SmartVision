//
// Created by bin zhang on 2019/1/10.
//

#ifndef INNERPROC_INNER_DEVICE_MGR_H
#define INNERPROC_INNER_DEVICE_MGR_H

#include <memory>

#include "device.h"
#include "service.h"
#include "sensor.h"

/**
 * @brief 室内设备连接管理器
 */
class InnerDeviceManager: Service { // ,std::enable_shared_from_this<InnerConnectionManager>{
	std::list< InnerDevice::Ptr >   devices_;
	std::shared_ptr<SocketServer>   sockserver_;
	
	Connection::Ptr  calling_conn_; //  当前呼叫中的连接
public:
	static std::shared_ptr<InnerDeviceManager>& instance(){
		static std::shared_ptr<InnerDeviceManager> handle ;
		if(!handle.get()){
			handle = new InnerDeviceManager;
		}
	}
	
	bool  init(const Config& cfgs);
	
	void open();
	void close();
	
//	void onCallComeIn(const CallInfo& callinfo,std::shared_ptr<Connection>& conn);
	
//	void onMessage(const std::shared_ptr<Message>& message,const std::share_ptr<InnerDevice>& device);
	void onJoinFamily(const std::shared_ptr<MessageJoinFamily> & msg,Connection::Ptr& conn);
	void onCallOut(const std::shared_ptr<MessageCallOut>  & msg ,Connection::Ptr& conn);
	void onCallEnd(const std::shared_ptr<MessageCallEnd>  & msg ,Connection::Ptr& conn);
	void onCallKeep(const std::shared_ptr<MessageCallKeep>  & msg ,Connection::Ptr& conn);
	void onCallReject(const std::shared_ptr<MessageCallReject>  & msg ,Connection::Ptr& conn);
	void onCallAccept(const std::shared_ptr<MessageCallAccept>  & msg ,Connection::Ptr& conn);
	
	
	void onConnected(const std::shared_ptr<Connection> & conn);
	void onDisconnected(const std::shared_ptr<Connection> & conn);
	
	bool isBusy();
	void postAlarm(const std::shared_ptr<SensorAlarmInfo> & alarm);     /*!< 广播报警到连接的室内设备 */
	bool callIn(const std::shared_ptr<CallRequestIn>& req);     /*!< 呼叫进入 */
	
	void endCall();     // 远端结束呼叫   (app 发送挂断)
	void keepCall();    // 远端保持呼叫线路 （由app定时发送)
	void rejectCall();  // 远端拒接
	void acceptCall();  // 远端接听
protected:
	void callOut(CallInfo& call,std::shared_ptr<InnerDevice>& device);	/*!<发起对外呼叫 */
};




#endif //INNERPROC_INNER_DEVICE_MGR_H
