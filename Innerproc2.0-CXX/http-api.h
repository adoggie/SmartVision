#ifndef INNERPROC_HTTP_API_H
#define INNERPROC_HTTP_API_H

#include "base.h"
#include <jsoncpp/json/json.h>

struct BoxStatusInfo{
	std::time_t  time;
	std::string  ver;
	std::uint32_t  fds;
	std::uint32_t  threads;
	std::uint32_t  mem_rss;
	std::uint32_t  mem_free;
	std::uint32_t  disk_free;
	int        outbox_net;
	int        propserver_net;
	std::string    net_ip;      // 小区网ip
	std::uint16_t  net_call_port;
	
	std::string    family_ip;   // 家庭内网ip
	std::uint16_t  family_call_port;
	
	std::string propserver_url;
	std::string http_proxy_url;
	std::string http_admin_url;
	
	int alarm_enable;
	int watchdog_enable;
	
	Json::Value jsonValues(){
		Json::Value values;
		values["time"] = Json::Value::UInt64 (time);
		values["ver"] = ver;
		values["fds"] = fds;
		values["threads"] = threads;
		values["mem_rss"] = mem_rss;
		values["mem_free"] = mem_free;
		values["disk_free"] = disk_free;
		values["outbox_net"] = outbox_net;
		values["proserver_net"] = propserver_net;
		values["net_ip"] = net_ip;
		values["net_call_port"] = net_call_port;
		values["family_ip"] = family_ip;
		values["family_call_port"] = family_call_port;
		
		values["propserver_url"] = propserver_url;
		values["http_proxy_url"] = http_proxy_url;
		values["http_admin_url"] = http_admin_url;
		values["alarm_enable"] = alarm_enable;
		values["watchdog_enable"] = watchdog_enable;
		return values;
	}
};


struct BoxDiscoverInfo{
	std::time_t  time;
	std::string  ver;
	std::string  service_api;
	std::string  server_api;
	std::string  push_address;
};


#define HTTP_JSON_RESULT_STATUS_OK 0
#define HTTP_JSON_RESULT_STATUS_ERROR 1




#endif