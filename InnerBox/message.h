//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_MESSAGE_H
#define INNERPROC_MESSAGE_H

#include "base.h"
#include <jsoncpp/json/json.h>

//struct Message{
//	typedef std::shared_ptr<Message> Ptr;
//};
//typedef std::list< std::shared_ptr<Message> > MessageList_t;
//

#define MESSAGE_JOIN_FAMILY     "join_family"       // 室内app加入主机
#define MESSAGE_JOIN_REJECT     "join_reject"       // 加入拒绝
#define MESSAGE_JOIN_ACCEPT     "join_accept"       // 同意加入
#define MESSAGE_CALL_REQ        "call_req"          // 呼叫请求
#define MESSAGE_CALL_ACCEPT     "call_accept"       // 呼叫接听
#define MESSAGE_CALL_REJECT     "call_reject"       // 呼叫拒绝
#define MESSAGE_CALL_END            "call_end"      // 呼叫终止
#define MESSAGE_CALL_KEEP           "call_keep"     // 呼叫保持
#define MESSAGE_HEARTBEAR      "heartbeat"     // 系统之间的连接心跳保持
#define MESSAGE_EMERGENCY      "emergency"     // 室内主机上报app报警信息
#define MESSAGE_OPEN_DOOR      "open_door"     // 室内主机App下发开门指令

class MessageJsonParser;

struct CallEndpoint{
	std::string 	ip;
	unsigned short 	port;
//	std::string 	room_id;
	std::string 	id;
	CallPeerType 	type;				/*!<呼叫对点类型 */
	std::string 	audio_stream_id;
	std::string		video_stream_id;
};

class Message {
protected:
	std::string id_;
	std::string name_;
	PropertyMap values_;
	
	friend class MessageJsonParser;
public:
	Message(){}
	Message(const std::string & name):name_(name){}
	virtual ~Message(){}
public:
	typedef std::shared_ptr<Message> Ptr;
	
	boost::any value(const std::string& name,boost::any def_=boost::any()) ;
	std::string getValueString(const std::string&name , const std::string& def_="");
	std::string& name() { return name_;}
	virtual Json::Value values(){ return Json::Value();}
	virtual  std::string marshall();

//	virtual bool unmarshall(const Json::Value& root){ return false;}
};


struct MessageJoinFamily:Message{
//	static const std::string NAME="join_family";
	
	std::string token;
	std::string id;
	std::string type;
	MessageJoinFamily():Message(MESSAGE_JOIN_FAMILY){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root);
	
	
};

struct MessageJoinReject:Message{
//	static const std::string NAME="join_reject";
	std::string reason;
	MessageJoinReject():Message(MESSAGE_JOIN_REJECT){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root);

	Json::Value values();
};

struct MessageJoinAccept:Message{
//	static const std::string NAME="join_accept";
	std::string room_id;
	MessageJoinAccept():Message( MESSAGE_JOIN_ACCEPT ){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root);

    Json::Value values();

};

struct MessageCall:Message{
	std::string  	sid;
	CallEndpoint	src;
	CallEndpoint	dest;

	MessageCall(){
	    name_ = MESSAGE_CALL_REQ;
	}
    Json::Value values();

    static std::shared_ptr<Message> parse(const Json::Value& root);
};


struct MessageCallAccept:Message{

	MessageCallAccept():Message(MESSAGE_CALL_ACCEPT){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root){
        std::shared_ptr<Message> msg;
        if( root["name"].asString() ==  MESSAGE_CALL_ACCEPT){
            msg = std::make_shared<MessageCallAccept>();
        }
        return msg;
	}
};

struct MessageCallReject:Message{
	enum class Reason{
		undefined,
		line_busy,
		call_denied, // 禁止呼叫
		app_offline,
	};
	Reason 		code;
	std::string message;				/*<! 原因*/
	
	MessageCallReject():Message(MESSAGE_CALL_REJECT){
	}
	
	MessageCallReject(Reason code_,const std::string message_=""):MessageCallReject(){
		code = code_;
		message = message_;
	}
	
	Json::Value values();
	
	static std::shared_ptr<Message> parse(const Json::Value& root);
};

struct MessageCallEnd:Message{

	MessageCallEnd():Message(MESSAGE_CALL_END){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root){
        std::shared_ptr<Message> msg;
        if( root["name"].asString() ==  MESSAGE_CALL_END ){
            msg = std::make_shared<MessageCallEnd>();
        }
        return msg;
	}
};


/* call_keep()
 主叫开始呼叫就连续发送 call_keep 信号，保持连接有效。
 被叫连接建立也bi需连续发送 call_keep , 即使用户并没有应答
 */
struct MessageCallKeep:Message{
	std::string sid;
	MessageCallKeep():Message(MESSAGE_CALL_KEEP){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root);
	virtual Json::Value values();
};

struct MessageHeartbeat:Message{
	MessageHeartbeat():Message(MESSAGE_HEARTBEAR){
	
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root);
};

//struct MessageAlarm:Message{
//	MessageAlarm():Message("alarm"){
//
//	}
//
//	static std::shared_ptr<Message> parse(const Json::Value& root);
//};

// 报警消息
struct MessageEmergency:Message{
    int port;
    std::string name;
    std::string message;

	MessageEmergency():Message(MESSAGE_EMERGENCY){
	
	}

	Json::Value values(){
		Json::Value values;
		values["port"] = port;
		values["name"] = name;
		values["message"] = message;
		return values;
	}
	static std::shared_ptr<Message> parse(const Json::Value& root);
};


// 开门消息
struct MessageOpenDoor:Message{
    MessageOpenDoor():Message(MESSAGE_OPEN_DOOR){

    }

    static std::shared_ptr<Message> parse(const Json::Value& root){
        std::shared_ptr<Message> msg;
        if( root["name"].asString() ==  MESSAGE_OPEN_DOOR){
            msg = std::make_shared<MessageOpenDoor>();
        }
        return msg;
    }
};


// 消息解释器
class MessageJsonParser{
public:
	static Message::Ptr parse(const char * data,size_t size);
	
};






#endif //INNERPROC_MESSAGE_H
