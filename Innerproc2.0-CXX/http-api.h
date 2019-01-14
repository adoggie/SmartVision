#ifndef INNERPROC_HTTP_API_H
#define INNERPROC_HTTP_API_H

#include "base.h"

struct BoxStatusInfo{
	std::time_t  time;
	std::string  ver;
	std::size_t  fds;
	std::size_t  threads;
	std::size_t  mem_rss;
	std::size_t  mem_free;
	std::size_t  disk_free;
	char        outbox_net;
	char        propserver_net;
	std::string    net_ip;      // 小区网ip
	std::string    family_ip;   // 家庭内网ip
};


struct BoxDiscoverInfo{
	std::time_t  time;
	std::string  ver;
	std::string  service_api;
	std::string  server_api;
	std::string  push_address;
};


#endif