#include "message.h"
#include <jsoncpp/json/json.h>

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
 
typedef std::function< std::shared_ptr<Message> (const Json::Value& root) > ParseFunc;

std::vector<ParseFunc> parse_func_list={
	MessageJoinFamily::parse,
	MessageJoinReject::parse,
	MessageJoinAccept::parse,
	MessageCall::parse,
	MessageCallIn::parse,
	MessageCallOut::parse,
	MessageCallAccept::parse,
	MessageCallReject::parse,
	MessageCallEnd::parse,
	MessageCallKeep::parse,
//	MessageLineBusy::parse,
	MessageHeartbeat::parse,
	MessageEmergency::parse,
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
	if( root["name"].asString() ==  "join_family"){
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
	return std::shared_ptr<MessageJoinReject>();
}

std::shared_ptr<Message> MessageHeartbeat::parse(const Json::Value& root){
	std::shared_ptr<Message> msg;
	if( root["name"].asString() == "heartbeat"){
		msg = std::make_shared<MessageHeartbeat>();
	}
	return msg;
}

std::shared_ptr<Message> MessageCallReject::parse(const Json::Value& root){
	std::shared_ptr<Message> result;
	if( root["name"].asString() == "call_reject"){
		auto msg = std::make_shared<MessageCallReject>();
		Json::Value values = root["values"];
		msg->code =  (MessageCallReject::Reason)values.get("code",(int)MessageCallReject::Reason::undefined).asInt();
		msg->message = values.get("message","").asString();
		result = msg;
	}
	return result;
}



std::shared_ptr<Message> MessageAlarm::parse(const Json::Value& root){
	
	return std::shared_ptr<Message>();
}


std::shared_ptr<Message> MessageEmergency::parse(const Json::Value& root) {
	
	return std::shared_ptr<Message>();
}


Json::Value MessageCallIn::values(){
	Json::Value value;
	value["sid"] = "";
	Json::Value src,dest;
	src["id"] =  this->src.id;
	src["type"] = this->src.type;
	src["ip"] = this->src.ip;
	src["port"] = this->src.port;
	src["stream_audio_url"] = this->src.stream_audio_url;
	src["stream_video_url"] = this->src.stream_video_url;
	
	dest["id"] =  this->dest.id;
	dest["type"] = this->dest.type;
	dest["ip"] = this->dest.ip;
	dest["port"] = this->dest.port;
	dest["stream_audio_url"] = this->dest.stream_audio_url;
	dest["stream_video_url"] = this->dest.stream_video_url;
	
	value["src"] = src;
	value["dest"] = dest;
	
	return value;
}

/*
 *
 *
 * http://open-source-parsers.github.io/jsoncpp-docs/doxygen/index.html
 *
 */



