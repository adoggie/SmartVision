
#include "http.h"

#include "mongoose.h"
#include <thread>
#include <boost/algorithm/string.hpp>

#include "InnerController.h"
#include "app.h"
#include "error.h"

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


HttpHeaders_t defaultResponseHeaders(){
	return {
			{"Content-Type","application/json"},
			{"Transfer-Encoding","chunked"}
	};
}

Json::Value defaultResponseJsonData(int status=HTTP_JSON_RESULT_STATUS_OK,int errcode = Error_NoError,const char* errmsg=""){
	Json::Value data;
	data["status"] = status;
	data["errcode"] = errcode;
	data["errmsg"] = errmsg;
	if(errmsg == std::string("")){
		data["errmsg"] =  ErrorDefs.at(errcode);
	}
	return data;
}


Json::Value errorResponseJsonData(int errcode = Error_NoError,const char* errmsg=""){
	Json::Value data;
	data["status"] = HTTP_JSON_RESULT_STATUS_ERROR;
	data["errcode"] = errcode;
	data["errmsg"] = errmsg;
	if(errmsg == std::string("")){
		data["errmsg"] =  ErrorDefs.at(errcode);
	}
	return data;
}


void send_http_response(struct mg_connection *nc,const std::string& text,const HttpHeaders_t& headers){
//	mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
	mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\n");
	
	
	for(auto p:headers) {
		mg_printf(nc, "%s: %s\r\n", p.first.c_str(),p.second.c_str());
	}
	mg_printf(nc, "%s", "\r\n");
	
	mg_send_http_chunk(nc, text.c_str(), text.size());
	mg_send_http_chunk(nc, "", 0);
}

void HttpService::handle_innerdevice_register(struct mg_connection *nc, struct http_message *hm ){

}


void HttpService::handle_innerdevice_discover(struct mg_connection *nc, struct http_message *hm ){

}

void HttpService::handle_innerdevice_list(struct mg_connection *nc, struct http_message *hm ){

}

void HttpService::handle_status_query(struct mg_connection *nc, struct http_message *hm ){
	Json::Value data = defaultResponseJsonData();
	data["result"] = InnerController::instance()->getStatusInfo();
	std::string text = data.toStyledString();
	send_http_response(nc,text,defaultResponseHeaders());
}

// 删除注册的室内机
void HttpService::handle_innerdevice_remove(struct mg_connection *nc, struct http_message *hm ){
	std::string token ;
	struct mg_str* str = mg_get_http_header(hm,"token");
}

/**
 * check_auth
 * @param hm
 * @param code
 * @return
 * @remark token的尾部32字节是签名校验码(md5),
 * 	if md5(secret_key,token[0:-32]) == token[-32:]
 * 		ok(..)
 * 	else
 * 	    error(..)
 *
 *  token 可携带设备的相关信息，采用base64编码
 */
bool HttpService::check_auth(struct http_message *hm,const std::string& code ){
	std::string token ;
	struct mg_str* str = mg_get_http_header(hm,"token");
	return  true;
	if(!str){
		Application::instance()->getLogger().error("check_auth() ,detail: token is missing.");
		return false;
	}
	token.assign(str->p,str->len);
	if( cfgs_.get_string(code) == token){
		return true;
	}
	
	return false;
}

/*
设备登录
参数：
  token - 用户授权码
返回： smartbox 运行接口参数
  - proxy_url 物业服务器api访问地址
  - access_url smartbox api访问地址
  - call_address  呼叫连接地址
*/
void handle_innerdevice_login(struct mg_connection *nc, struct http_message *hm ){

}

//修改防区密码
void HttpService::handle_seczone_passwd_set(struct mg_connection *nc, struct http_message *hm ){
	Json::Value data = defaultResponseJsonData();
	
	if( !check_auth(hm,"inner_token")){
		data = errorResponseJsonData(Error_TokenInvalid);
	}else{
//		data["result"] = InnerController::instance()->getStatusInfo();
	}
	
	std::string text = data.toStyledString();
	send_http_response(nc,text,defaultResponseHeaders());
}

void HttpService::ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
	struct http_message *hm = (struct http_message *) ev_data;
	HttpService* http = HttpService::instance().get();
	switch (ev) {
		case MG_EV_HTTP_REQUEST:
			if (mg_vcmp(&hm->uri, "/smartbox/discover") == 0) {
				http->handle_innerdevice_discover(nc, hm); /* Handle RESTful call */
			}else if (mg_vcmp(&hm->uri, "/smartbox/api/status") == 0) {
				http->handle_status_query(nc, hm);
			}else if (mg_vcmp(&hm->uri, "/smartbox/api/inner-device/list") == 0) {
				http->handle_innerdevice_list(nc, hm);

			}else if (mg_vcmp(&hm->uri, "/smartbox/api/inner-device") == 0) {
				std::string method = hm->method.p;
				boost::to_lower(method);
				if(method == std::string("post")){ // 注册
					http->handle_innerdevice_register(nc,hm);
				}else if( method == "delete"){ // 注销
					http->handle_innerdevice_remove(nc,hm);
				}
			}else if (mg_vcmp(&hm->uri, "/smartbox/api/seczone/passwd") == 0) { //防区密码设置
				http->handle_seczone_passwd_set(nc,hm);
			}else if (mg_vcmp(&hm->uri, "/smartbox/api/login") == 0) { // 设备登录
			
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
	cfgs_ = cfgs;
	return true;
}

bool HttpService::open(){
	
	int i;
	char *cp;
	const char *err_str;
	mg_mgr_init(&mgr, NULL);
	s_http_server_opts.document_root = "/tmp/smartbox/http";
	std::string http_port  = cfgs_.get_string("http.port","8000");
	
//	std::function< void (struct mg_connection *, int , void *) >  fx =std::bind( &HttpService::ev_handler,_1,_2,_3);
//	auto  fx =std::bind( &HttpService::ev_handler,_1,_2,_3);
	
//
//	nc = mg_bind_opt(&mgr, s_http_port,std::bind(&HttpService::ev_handler,this,_1,_2,_3), bind_opts);
	nc = mg_bind_opt(&mgr, http_port.c_str(),HttpService::ev_handler, bind_opts);
//	nc = mg_bind_opt(&mgr, s_http_port,(mg_event_handler_t)fx, bind_opts);
	if (nc == NULL) {
		fprintf(stderr, "Error starting server on port %s: %s\n", http_port.c_str(),
		        *bind_opts.error_string);
		return false;
	}
	
	mg_set_protocol_http_websocket(nc);
	s_http_server_opts.enable_directory_listing = "yes";
	
	printf("Starting RESTful server on port %s, serving %s\n", s_http_port,
	       s_http_server_opts.document_root);
	thread_ = std::make_shared<std::thread>( std::bind(&HttpService::thread_run,this));
	
	return true;
}

void HttpService::close(){
	running_ = false;
	thread_->join();
}

void HttpService::run() {
//	thread_->join();
}