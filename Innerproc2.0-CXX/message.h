//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_MESSAGE_H
#define INNERPROC_MESSAGE_H

#include "base.h"

//struct Message{
//	typedef std::shared_ptr<Message> Ptr;
//};
//typedef std::list< std::shared_ptr<Message> > MessageList_t;
//


struct CallEndpoint{
	std::string 	ip;
	unsigned short 	port;
	std::string 	room_id;
	CallPeerType 	type;				/*!<呼叫对点类型 */
	std::string 	stream_video_url;
	std::string		stream_audio_url;
};

class Message {
protected:
	std::string id_;
	std::string name_;
	PropertyMap values_;
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
	virtual  std::string marshall(){ return "";}
};


struct MessageJoinFamily:Message{
	std::string token;
	std::string device_id;
	std::string device_type;
	MessageJoinFamily():Message("join_family"){
	}
};

struct MessageJoinReject:Message{
	std::string reason;
	MessageJoinReject():Message("join_reject"){
	}
};

struct MessageJoinAccept:Message{
	MessageJoinAccept():Message("join_accept"){
	}
};

struct MessageCall:Message{
	CallEndpoint	src;
	CallEndpoint	dest;
//	virtual ~MessageCall(){}

};

struct MessageCallIn:MessageCall{
	MessageCallIn(){
		name_ = "call_in";
	}
};

struct MessageCallOut:MessageCall{
	MessageCallOut(){
		name_ = "call_out";
	}
};


struct MessageCallAccept:Message{
	MessageCallAccept():Message("call_accept"){
	}
};

struct MessageCallReject:Message{
	MessageCallReject():Message("call_reject"){
	}
};

struct MessageCallEnd:Message{
	MessageCallEnd():Message("call_end"){
	}
};

struct MessageCallKeep:Message{
	MessageCallKeep():Message("call_keep"){
	}
};

struct MessageLineBusy:Message{
	MessageLineBusy():Message("line_busy"){
	
	}
};

struct MessageHeartbeat:Message{
	MessageHeartbeat():Message("heartbeat"){
	
	}
};

class MessageJsonParser{
public:
	static Message::Ptr parse(const char * data,size_t size);
	
};






#endif //INNERPROC_MESSAGE_H
