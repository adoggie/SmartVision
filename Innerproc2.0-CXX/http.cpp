
#include "http.h"


/**
1. 服务发现
  url: api/smartbox/discover
  return:
    - time : 20191123 12:00:12
    - ver : 1.0
    - service_api: http://192.168.1.34:8088/api/smartbox/
    - server_api: http://192.168.1.34:5568/api/smartbox/
    - push_address: tcp://192.168.1.34:6453
 
 2. 运行状态查询
   url : api/smartbox/status
   method: get
   return:
     - time
     - ver
     - fds  打开文件数
     - threads 运行的线程数
     - mem_rss 占用内存
     - mem_free 系统空余内存
     - disk_free 磁盘空闲
     - outbox_net  与室外机连接状态 0:未连接, 1:已连接
     - propserver_net  与物业服务器链接状态
     - ips			当前主机ip ,  ip1,ip2 表示小区网ip和家庭网口ip

 3. 注册室内设备
 	url: api/smartbox/inner-device/
 	method: post
 	params:
 	  - device_id
 	  - device_type  ios,android,screen
 	  - box_auth_code	室内机的授权码，在室内屏可查询获得室内机的授权码，室内屏安装时直接读取box内的设置参数
 	return:
 	  - token  访问设备的授权码

 3.1 查询注册室内设备列表
   url: api/smartbox/inner-device/list
   method: get
   params:
     - token  访问授权码
   return:
     - device_id
     - device_type
     - register_time

 3.2 删除设备
   url: api/smartbox/inner-device
   metod: delete
   params:
     - token  访问授权码
     - device_id
   return:
   
 
 4. 设备重启
   url: api/smartbox/reboot
   metod: post
   params:
     - token  访问授权码

 
 物业服务端接口
 --------
 
 1. 设备smartbox 登录服务器
   url: api/smartbox/login
   method: post
   params:
     - device_id
     - ip
     - signature

 2. 获取室内机列表
   url： api/smartbox/list
   method: get
   params:
      - token
      - ver
      - ip  家庭wifi地址或smartbox的小区网地址
 3. 获取室外机列表
 
 
 */
 
#include "mongoose.h"
#include <thread>

static const char *s_http_port = "8000";
static struct mg_serve_http_opts s_http_server_opts;
struct mg_mgr mgr;
struct mg_connection *nc;
struct mg_bind_opts bind_opts;

static void handle_sum_call(struct mg_connection *nc, struct http_message *hm) {
	char n1[100], n2[100];
	double result;
	
	/* Get form variables */
	mg_get_http_var(&hm->body, "n1", n1, sizeof(n1));
	mg_get_http_var(&hm->body, "n2", n2, sizeof(n2));
	
	/* Send headers */
	mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
	
	/* Compute the result and send it back as a JSON object */
	result = strtod(n1, NULL) + strtod(n2, NULL);
	mg_printf_http_chunk(nc, "{ \"result\": %lf }", result);
	mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
}

void HttpService::ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
	struct http_message *hm = (struct http_message *) ev_data;
	
	switch (ev) {
		case MG_EV_HTTP_REQUEST:
			if (mg_vcmp(&hm->uri, "/api/v1/sum") == 0) {
				handle_sum_call(nc, hm); /* Handle RESTful call */
			} else if (mg_vcmp(&hm->uri, "/printcontent") == 0) {
				char buf[100] = {0};
				memcpy(buf, hm->body.p,
				       sizeof(buf) - 1 < hm->body.len ? sizeof(buf) - 1 : hm->body.len);
				printf("%s\n", buf);
			} else {
				mg_serve_http(nc, hm, s_http_server_opts); /* Serve static content */
			}
			break;
		default:
			break;
	}
}

void HttpService::thread_run() {
	
	
	running_ = true;
	while( running_ ){
		mg_mgr_poll(&mgr, 1000);
	}
	mg_mgr_free(&mgr);
	
}

bool  HttpService::init(const Config& cfgs){
	
	return true;
}

bool HttpService::open(){
	
	int i;
	char *cp;
	const char *err_str;
	mg_mgr_init(&mgr, NULL);
	s_http_server_opts.document_root = "/var/smartbox/http";
	std::function< void (struct mg_connection *, int , void *) >  fx =std::bind( &HttpService::ev_handler,this);
	
	nc = mg_bind_opt(&mgr, s_http_port,(mg_event_handler_t)fx, bind_opts);
	if (nc == NULL) {
		fprintf(stderr, "Error starting server on port %s: %s\n", s_http_port,
		        *bind_opts.error_string);
		return false;
	}
	
	mg_set_protocol_http_websocket(nc);
	s_http_server_opts.enable_directory_listing = "yes";
	
	printf("Starting RESTful server on port %s, serving %s\n", s_http_port,
	       s_http_server_opts.document_root);
	thread_ = std::make_shared<std::thread>( std::bind(&HttpService::thread_run,this));
}

void HttpService::close(){

}

void HttpService::run() {
	thread_->join();
}