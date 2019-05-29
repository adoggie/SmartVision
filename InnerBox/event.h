//
// Created by scott on 2019/3/17.
//

#ifndef INNERPROC_EVENT_H
#define INNERPROC_EVENT_H

#include "base.h"
#include <jsoncpp/json/json.h>
#include <boost/lexical_cast.hpp>

#define EVENT_SYSTEM_START  "start"
#define EVENT_EMERGENCY  "emergency"
#define EVENT_ERROR  "error"
#define EVENT_CALL_IN  "callin"
#define EVENT_CALL_OUT  "callout"
#define EVENT_JOIN_FAMILY  "join_family"
#define EVENT_CALL_REJECT  "call_reject"
#define EVENT_MCU_HB_OFFLINE  "mcu_offline" // 检测到单片机控制模块失效


//设备运行事件信息
struct Event{
    int             dev_type;  	//设备类型 1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App
    std::string     dev_id;     // 		设备编号，室内屏设备即认证token
    std::string     event;	    //		事件名称
    std::time_t     time;		//		时间
    std::string     content;	//		事件内容

    Event(){}
    Event( const std::string& _event,const std::string& _content, const std::string& _dev_id="",int _dev_type = CallPeerType::INNER_BOX);
    virtual  ~Event(){}

    Json::Value jsonValue() const;

    PropertyStringMap httpValues() const;
};

struct Event_SystemStart:Event{
    Event_SystemStart():Event(EVENT_SYSTEM_START,""){
    }
};


struct Event_Emergency:Event{
    int     port;
    std::string     name;
    std::string     detail;
    std::string     rand_key;   //随机报警流水号，用于报警开门时的身份确认

    Event_Emergency(){}
    Event_Emergency(int port_,const std::string& name_,const std::string& detail_):Event(EVENT_EMERGENCY,""){
        port = port_;
        name = name_;
        detail = detail_;
    }

    PropertyStringMap httpValues() const;
};

#endif //INNERPROC_EVENT_H
