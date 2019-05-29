#include "message.h"
#include <jsoncpp/json/json.h>
#include "app.h"
/**
 * parse()
 * @param data
 * @param size
 * @return
 *
 * {
 * 	 name: line_busy,
 * 	 id: 322213123,
 *   values:{
 *		url: xxx,
 *		type: 10,
 *		...
 *   }
 * }
 */
#define SPS "\0"

typedef std::function< std::shared_ptr<Message> (const Json::Value& root) > ParseFunc;

std::vector<ParseFunc> parse_func_list={
	MessageJoinFamily::parse,
	MessageJoinReject::parse,
	MessageJoinAccept::parse,
	MessageCall::parse,
//	MessageCallIn::parse,
//	MessageCallOut::parse,
	MessageCallAccept::parse,
	MessageCallReject::parse,
	MessageCallEnd::parse,
	MessageCallKeep::parse,
//	MessageLineBusy::parse,
	MessageHeartbeat::parse,
    MessageOpenDoor::parse,
//	MessageEmergency::parse,
};

Message::Ptr MessageJsonParser::parse(const char * data,size_t size){
	Json::Reader reader;
	Json::Value root;
	Message::Ptr msg;

	if (reader.parse(data, root)){
		for(auto func:parse_func_list){
			msg = func(root);
			if(msg){
				msg->id_ = root["id"].asString();
				break;
			}
		}
	}

	return msg;
}


std::shared_ptr<Message> MessageJoinFamily::parse(const Json::Value& root){
	std::shared_ptr<Message> result;
	if( root["name"].asString() ==  MESSAGE_JOIN_FAMILY){
		std::shared_ptr<MessageJoinFamily> msg = std::make_shared<MessageJoinFamily>();
		Json::Value values = root["values"];
		msg->token = values["token"].asString();
		msg->id =  values["id"].asString();
		msg->type = values["type"].asString();
		result = msg ;
	}
	return result;
}

std::shared_ptr<Message> MessageJoinReject::parse(const Json::Value& root){
	std::shared_ptr<MessageJoinReject>  msg;
	if( root["name"].asString() ==  MESSAGE_JOIN_REJECT){
	    msg = std::make_shared<MessageJoinReject>();
        Json::Value values = root["values"];
	    msg->reason = values["reason"].asString();
	}
	return msg;
}


Json::Value MessageJoinReject::values(){
    Json::Value node;
    node["reason"] = reason;
    return node;
}


std::shared_ptr<Message> MessageHeartbeat::parse(const Json::Value& root){
	std::shared_ptr<Message> msg;
	if( root["name"].asString() == MESSAGE_HEARTBEAR){
		msg = std::make_shared<MessageHeartbeat>();
	}
	return msg;
}


//std::shared_ptr<Message> MessageAlarm::parse(const Json::Value& root){
//
//	return std::shared_ptr<Message>();
//}

// 室内主机只发送不接收
std::shared_ptr<Message> MessageEmergency::parse(const Json::Value& root) {
	return std::shared_ptr<Message>();
}


Json::Value MessageCall::values(){
	Json::Value value;
	value["sid"] = this->sid;
	Json::Value src,dest;
	src["id"] =  this->src.id;
	src["type"] = this->src.type;
	src["ip"] = this->src.ip;
	src["port"] = this->src.port;
	src["audio_stream_id"] = this->src.audio_stream_id;
	src["video_stream_id"] = this->src.video_stream_id;
	
	dest["id"] =  this->dest.id;
	dest["type"] = this->dest.type;
	dest["ip"] = this->dest.ip;
	dest["port"] = this->dest.port;
	dest["audio_stream_id"] = this->dest.audio_stream_id;
	dest["video_stream_id"] = this->dest.video_stream_id;
	
	value["src"] = src;
	value["dest"] = dest;
	
	return value;
}


std::shared_ptr<Message> MessageCall::parse(const Json::Value& root){
	std::shared_ptr<Message> result;
	if( root["name"].asString() ==  MESSAGE_CALL_REQ){
        Json::Value values = root["values"];
        if(!values.isNull()){
            std::puts("values is not nul");
        }
		auto msg = std::make_shared<MessageCall>();
		msg->sid = values["sid"].asString();
		Json::Value src = values["src"];
		Json::Value dest = values["dest"];
		if(src.isNull() == false) {
            msg->src.id = src["id"].asString();
            msg->src.type = (CallPeerType) src["type"].asInt();
            msg->src.ip = src["ip"].asString();
            msg->src.port = src["port"].asInt();
            msg->src.audio_stream_id = src["audio_stream_id"].asString();
            msg->src.video_stream_id = src["video_stream_id"].asString();
        }
		msg->dest.id = dest["id"].asString();
		msg->dest.type = (CallPeerType )dest["type"].asInt();
		msg->dest.ip = dest["ip"].asString();
		msg->dest.port = dest["port"].asInt();
		msg->dest.audio_stream_id = dest["audio_stream_id"].asString();
		msg->dest.video_stream_id = dest["video_stream_id"].asString();


		result = msg;
	}
	return result;
}

Json::Value MessageCallReject::values(){
    Json::Value root;
    root["code"] = (int) code;
    root["message"] = message;
    return root;
}


std::shared_ptr<Message> MessageCallReject::parse(const Json::Value& root){
    std::shared_ptr<Message> result;
    if( root["name"].asString() == MESSAGE_CALL_REJECT){
        auto msg = std::make_shared<MessageCallReject>();
        Json::Value values = root["values"];
        msg->code =  (MessageCallReject::Reason)values.get("code",(int)MessageCallReject::Reason::undefined).asInt();
        msg->message = values.get("message","").asString();
        result = msg;
    }
    return result;
}


Json::Value MessageJoinAccept::values(){
    Json::Value vs;
    return vs ;
}

std::shared_ptr<Message> MessageJoinAccept::parse(const Json::Value& root){
    std::shared_ptr<Message> result;
    if( root["name"].asString() == MESSAGE_JOIN_ACCEPT){
        auto msg = std::make_shared<MessageJoinAccept>();
        Json::Value values = root["values"];
        msg->room_id = root["room_id"].asString();
        result = msg;
    }
    return result;
}


boost::any Message::value(const std::string& name,boost::any def_)  {
    try {
        return values_.at(name);
    }catch (...){

    }
    return def_;
}

std::string Message::getValueString(const std::string&name , const std::string& def_){
    try {
        return boost::any_cast<std::string>(this->value(name));
//		}catch (boost::bad_any_cast)
    }catch(...){
        return def_;
    }
}

std::shared_ptr<Message> MessageCallKeep::parse(const Json::Value& root){
	std::shared_ptr<Message> result;
	if( root["name"].asString() ==  MESSAGE_CALL_KEEP){
		auto msg = std::make_shared<MessageCallKeep>();
		Json::Value values = root["values"];
		msg->sid = values["sid"].asString();
		result = msg;
//		Application::instance()->getLogger().debug("CallKeep: sid = " + msg->sid);
	}
	return result;

}

Json::Value MessageCallKeep::values(){
	Json::Value root;
	root["sid"] = this->sid;
//	Application::instance()->getLogger().debug("MessageCallKeep::values , sid =" + this->sid);
	return root;
}

std::string Message::marshall(){
    Json::Value root;
    Json::Value arrayObj;
    Json::Value item;
    root["id"] = id_;
    root["name"] = name_;
    Json::Value values = this->values();
    if( !values.isNull()){
        root["values"] = values;
    }
    Application::instance()->getLogger().debug("Message Marshall:" + root.toStyledString());
    return root.toStyledString()  ;
}

/*
 *
 *
 * http://open-source-parsers.github.io/jsoncpp-docs/doxygen/index.html
 *
 */



