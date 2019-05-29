//
// Created by scott on 2019/3/17.
//

#include "event.h"
#include "InnerController.h"

Event::Event( const std::string& _event,const std::string& _content, const std::string& _dev_id,int _dev_type ){
    if(_dev_id.length() == 0){
        dev_id = InnerController::instance()->getDeviceUniqueID();
    }else{
        dev_id = _dev_id;
    }
    dev_type = _dev_type;

    event = _event;
    content = _content;
    time = std::time(NULL);
}


Json::Value Event::jsonValue() const{
    Json::Value value;
    value["dev_type"] = dev_type;
    value["dev_id"] = dev_id;
    value["event"] = event;
    value["time"] = (uint32_t )time;
    value["content"] = content;
    return value;
}

PropertyStringMap Event::httpValues() const{
    PropertyStringMap values;
    values["dev_type"] = boost::lexical_cast<std::string>((int)dev_type);
    values["dev_id"] = dev_id;
    values["event"] = event;
    values["time"] = boost::lexical_cast<std::string>((uint32_t )time);
    values["content"] = content;
    return  values;
}


PropertyStringMap Event_Emergency::httpValues() const{
    PropertyStringMap values = Event::httpValues();
//    Json::Value json;
    values["port"] = port;
    values["name"] = name;
    values["detail"] = detail;
    values["rand_key"] = detail;
//    values["content"] = json.toStyledString();
    return values;
}