//
// Created by scott on 2019/3/15.
//

#include "http-api.h"

Json::Value BoxStatusInfo::values(){
    Json::Value values;
    values["time"] = Json::Value::UInt64 (time);
    values["ver"] = ver;
    values["fds"] = fds;
    values["threads"] = threads;
    values["mem_rss"] = mem_rss;
//		values["mem_free"] = mem_free;
//		values["disk_free"] = disk_free;
    values["outbox_net"] = outbox_net;
    values["proserver_net"] = propserver_net;
    values["net_ip"] = net_ip;
    values["net_call_port"] = net_call_port;
    values["family_ip"] = family_ip;
    values["family_call_port"] = family_call_port;

    values["propserver_url"] = propserver_url;
//		values["http_proxy_url"] = http_proxy_url;
//		values["http_admin_url"] = http_admin_url;
    values["alarm_enable"] = alarm_enable;
    values["watchdog_enable"] = watchdog_enable;
    values["call_in_enable"] = call_in_enable;
    values["seczone_mode"] = seczone_mode;
    return values;
}