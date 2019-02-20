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

class MessageJsonParser;

struct CallEndpoint{
	std::string 	ip;
	unsigned short 	port;
	std::string 	room_id;
	std::string 	id;
	CallPeerType 	type;				/*!<呼叫对点类型 */
	std::string 	stream_video_url;
	std::string		stream_audio_url;
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
	
	boost::any value(const std::string& name,boost::any def_=boost::any())  {
		try {
			return values_.at(name);
		}catch (...){
		
		}
		return def_;
	}
	
	std::string getValueString(const std::string&name , const std::string& def_=""){
		try {
			return boost::any_cast<std::string>(this->value(name));
//		}catch (boost::bad_any_cast)
		}catch(...){
			return def_;
		}
	}
	
	std::string& name() { return name_;}
//	virtual bool serialize(Json::Value& root){
//		root["id"] = id_;
//
//		return true;
//	}
	virtual Json::Value values(){ return Json::Value();}
	
	virtual  std::string marshall(){
		Json::Value root;
		Json::Value arrayObj;
		Json::Value item;
		root["id"] = id_;
		root["name"] = name_;
		Json::Value values = this->values();
		if( !values.isNull()){
			root["values"] = values;
		}
		return root.toStyledString();
	}
	virtual bool unmarshall(const Json::Value& root){ return false;}
};


struct MessageJoinFamily:Message{
//	static const std::string NAME="join_family";
	
	std::string token;
	std::string id;
	std::string type;
	MessageJoinFamily():Message("join_family"){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root);
	
	
};

struct MessageJoinReject:Message{
//	static const std::string NAME="join_reject";
	std::string reason;
	MessageJoinReject():Message("join_reject"){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root);
	
};

struct MessageJoinAccept:Message{
//	static const std::string NAME="join_accept";
	std::string room_id;
	MessageJoinAccept():Message("join_accept"){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root){
		return std::shared_ptr<Message>();
	}
};

struct MessageCall:Message{
	CallEndpoint	src;
	CallEndpoint	dest;
//	virtual ~MessageCall(){}
	static std::shared_ptr<Message> parse(const Json::Value& root){
		return std::shared_ptr<Message>();
	}
};

	struct MessageCallIn:MessageCall{
	MessageCallIn(){
		name_ = "call_in";
	}
	virtual Json::Value values();
	
	static std::shared_ptr<Message> parse(const Json::Value& root){
		return std::shared_ptr<Message>();
	}
};

struct MessageCallOut:MessageCall{
	MessageCallOut(){
		name_ = "call_out";
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root){
		return std::shared_ptr<Message>();
	}
};


struct MessageCallAccept:Message{
	MessageCallAccept():Message("call_accept"){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root){
		return std::shared_ptr<Message>();
	}
};

struct MessageCallReject:Message{
	enum class Reason{
		undefined,
		line_busy
	};
	Reason 		code;
	std::string message;				/*<! 原因*/
	
	MessageCallReject():Message("call_reject"){
	}
	
	MessageCallReject(Reason code_,const std::string message_=""):MessageCallReject(){
		code = code_;
		message = message_;
	}
	
	Json::Value values(){
		Json::Value root;
		root["code"] = (int) code;
		root["message"] = message;
		return root;
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root);
};

struct MessageCallEnd:Message{
	MessageCallEnd():Message("call_end"){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root){
		return std::shared_ptr<Message>();
	}
};


/* call_keep()
 主叫开始呼叫就连续发送 call_keep 信号，保持连接有效。
 被叫连接建立也bi需连续发送 call_keep , 即使用户并没有应答
 */
struct MessageCallKeep:Message{
	MessageCallKeep():Message("call_keep"){
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root){
		return std::shared_ptr<Message>();
	}
};

//struct MessageLineBusy:Message{
//	MessageLineBusy():Message("line_busy"){
//
//	}
//
//	static std::shared_ptr<Message> parse(const Json::Value& root);
//};

struct MessageHeartbeat:Message{
	MessageHeartbeat():Message("heartbeat"){
	
	}
	
	static std::shared_ptr<Message> parse(const Json::Value& root);
};

struct MessageAlarm:Message{
	MessageAlarm():Message("alarm"){

	}

	static std::shared_ptr<Message> parse(const Json::Value& root);
};

// 报警消息
struct MessageEmergency:Message{
	MessageEmergency():Message("emergency"){
	
	}
	int port;
	std::string name;
	std::string message;
	
	
	Json::Value values(){
		Json::Value values;
		values["port"] = port;
		values["name"] = name;
		values["message"] = message;
		return values;
	}
	static std::shared_ptr<Message> parse(const Json::Value& root);
};


class MessageJsonParser{
public:
	static Message::Ptr parse(const char * data,size_t size);
	
};






#endif //INNERPROC_MESSAGE_H
