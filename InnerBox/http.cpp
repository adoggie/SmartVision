
#include "http.h"

#include "mongoose.h"
#include <thread>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <jsoncpp/json/json.h>

#include "InnerController.h"
#include "app.h"
#include "error.h"
#include "md5.hpp"
#include "base64.h"


// mongoose help
// https://cesanta.com/docs/overview/intro.html
// https://github.com/cesanta/mongoose/blob/master/examples/simple_crawler/simple_crawler.c
// https://github.com/Gregwar/mongoose-cpp


/*
 * A - 室内屏  B - 室内主机  C - 新增室内设备(手机)  D - 物业服务器 E - 室外机 F - 物业App
 *
设备注册:
 	A请求B生成新设备注册地址 ( http://ip:port/../token ) , token生成需添加时间阈值，自签名之后返回给A，A生成QR码
	C 扫描 QR码，获得注册url，继而发起对B的指定ur的注册请求(携带 sn,type,os等设备相关信息),B返回C的认证码T.
	(后续所有C访问B的请求均需携带T作为有效身份识别)

注册设备查询和删除：
 A ，D 查询已注册到B的C类型设备记录 ，并可以删除指定的注册记录



 */
static const char *s_http_port = "8000";
static struct mg_serve_http_opts s_http_server_opts;
struct mg_mgr mgr;
//struct mg_connection *nc;
struct mg_bind_opts bind_opts;

//static void handle_sum_call(struct mg_connection *nc, struct http_message *hm) {
//	char n1[100], n2[100];
//	double result;
//
//	/* Get form variables */
//	mg_get_http_var(&hm->body, "n1", n1, sizeof(n1));
//	mg_get_http_var(&hm->body, "n2", n2, sizeof(n2));
//
//	/* Send headers */
//	mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
//
//	/* Compute the result and send it back as a JSON object */
//	result = strtod(n1, NULL) + strtod(n2, NULL);
//	mg_printf_http_chunk(nc, "{ \"result\": %lf }", result);
//	mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
//}

std::string http_get_var(struct http_message *hm,const std::string& name,const std::string& def=""){
	char buf[1200];
	int ret;

	std::string val;
	ret = mg_get_http_var(&hm->body, name.c_str(), buf, sizeof(buf));
	if(ret > 0 ){
		val = buf;
	}else{
	    val = def;
	}
	return val;
}

std::string http_get_query_var(struct http_message *hm,const std::string& name,const std::string& def=""){
    char buf[1200];
    int ret;

    std::string val;
    ret = mg_get_http_var(&hm->query_string, name.c_str(), buf, sizeof(buf));
    if(ret > 0 ){
        val = buf;
    }else{
        val = def;
    }
    return val;
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


void send_http_response_error(struct mg_connection *nc,int errcode = Error_NoError,const char* errmsg="") {
    Json::Value data = errorResponseJsonData(errcode,errmsg);
    std::string text = data.toStyledString();
    send_http_response(nc, text, defaultResponseHeaders());
}

void send_http_response_okay(struct mg_connection *nc,int errcode = Error_NoError,const char* errmsg="") {
    Json::Value data = defaultResponseJsonData();
    std::string text = data.toStyledString();
    send_http_response(nc, text, defaultResponseHeaders());
}

void send_http_response_result(Json::Value result,struct mg_connection *nc,int errcode = Error_NoError,const char* errmsg="") {
    Json::Value data = defaultResponseJsonData();
    data["result"] = result;
    std::string text = data.toStyledString();
    send_http_response(nc, text, defaultResponseHeaders());
}


/**
 * 手机在室内屏上获取qr码url之后请求smartbox进行设备注册
 * 输入:
 *   - sn  	设备硬件编码
 *   - type  phone,pad,other
 *   - os  android , ios
 *   - code 注册请求码
 *
 * @param hm
 */
void HttpService::handle_innerdevice_register(struct mg_connection *nc, struct http_message *hm ){
	std::string sn = http_get_var(hm,"sn");
	std::string type = http_get_var(hm,"type");
	std::string os = http_get_var(hm,"os");
	std::string code = http_get_var(hm,"code");

	std::string data ;

	if(sn.length() < 5 || type.length() ==0 || os.length() == 0 || code.length() == 0){
		send_http_response(nc,errorResponseJsonData(Error_ParameterInvalid,"Parameters Invalid").toStyledString(),defaultResponseHeaders());
		return ;
	}

	// 校验注册码
	if(Base64::Decode(code,&data) == false){
		send_http_response(nc,errorResponseJsonData(Error_ParameterInvalid,"Code Invalid").toStyledString(),defaultResponseHeaders());
		return ;
	}

	std::vector<std::string> values;
	boost::split(values, data,boost::is_any_of((",")));
	if(values.size() != 3){
		send_http_response(nc,errorResponseJsonData(Error_ParameterInvalid,"Code Invalid").toStyledString(),defaultResponseHeaders());
		return ;
	}
	std::string time,device_id,md5;
	time = values[0];
	device_id = values[1];
	md5 = values[2];
	std::string text;
	text = time + device_id;
	std::string digest = mongo::md5simpledigest(text);
	if(md5 != digest){
		send_http_response(nc,errorResponseJsonData(Error_ParameterInvalid,"Digest Check Failed.").toStyledString(),defaultResponseHeaders());
		return ;
	}

	//code 有效期最多30分钟
	std::time_t ts = boost::lexical_cast<std::time_t>(time.c_str());
	if( std::time(NULL) - ts > 3600/2){
		send_http_response(nc,errorResponseJsonData(Error_ParameterInvalid,"ReqCode Expired(30 mins) .").toStyledString(),defaultResponseHeaders());
		return ;
	}

	//生成 设备 身份码 token
	// M = box_id,screen_sn,screen_type,screen_os,screen_reg_time
	// D = md5( D )
	// token = base64( M+D )
	time = boost::lexical_cast<std::string>(ts);
	text = InnerController::instance()->getDeviceUniqueID() + "," + sn + "," + type + "," + os + "," + time;
	md5 = mongo::md5simpledigest(text);
	text+= "," + md5;
	std::string token;
	Base64::Encode(text,&token);

	// encode to json-response
	Json::Value json = defaultResponseJsonData();
	Json::Value result;
	result["token"] = token;
	result["net_ip"] = InnerController::instance()->settings().net_ip;
	result["family_ip"] = InnerController::instance()->settings().family_ip;
    result["room_id"] = InnerController::instance()->settings().room_id;
	json["result"] = result ;
	send_http_response(nc,json.toStyledString(),defaultResponseHeaders());
}

//// 目前不用
//void HttpService::handle_innerdevice_discover(struct mg_connection *nc, struct http_message *hm ){
//
//}
//
////列出当前注册的所有室内屏设备
//void HttpService::handle_innerdevice_list(struct mg_connection *nc, struct http_message *hm ){
//
//}

//查询设备运行状态
// 1.1
void HttpService::handle_status_query(struct mg_connection *nc, struct http_message *hm ){
	Json::Value data = defaultResponseJsonData();
	data["result"] = InnerController::instance()->getStatusInfo();
	std::string text = data.toStyledString();
	send_http_response(nc,text,defaultResponseHeaders());
}

// 删除注册的室内机
//void HttpService::handle_innerdevice_remove(struct mg_connection *nc, struct http_message *hm ){
//	std::string token ;
//	struct mg_str* str = mg_get_http_header(hm,"token");
//}

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
 *
 *  物业中心发起对室内机的查询请求，token是固定值
 *
 *
 */
bool HttpService::check_auth(struct mg_connection *nc,struct http_message *hm,const std::string& code ){
	std::string token ;
//	struct mg_str* str = mg_get_http_header(hm,"token");
    token = http_get_var(hm,"token");
	if ( cfgs_.get_string("advance_access_token") == token ){
	    return true ;
	}
    return true;

    Json::Value data = errorResponseJsonData(Error_TokenInvalid);
    std::string text = data.toStyledString();
    send_http_response(nc,text,defaultResponseHeaders());

	return  false;
//	if(!str){
//		Application::instance()->getLogger().error("check_auth() ,detail: token is missing.");
//		return false;
//	}
//	token.assign(str->p,str->len);
//	if( cfgs_.get_string(code) == token){
//		return true;
//	}
//
//	return false;
}


/*新设备注册时，室内屏从box请求临时注册code
 box 将box的硬件编码，当前时间进行哈希运算，base64编码之后作为注册code返回
 室内屏 将 box的http访问地址和code生成QR码供新设备扫码。 新设备扫码获得box的注册url，进而进行注册操作
 */
void HttpService::handle_innerdevice_reqcode(struct mg_connection *nc, struct http_message *hm ){
	std::time_t now = std::time(NULL);
	std::string device_id = InnerController::instance()->getDeviceUniqueID();
	std::string text = boost::lexical_cast< std::string >((std::uint32_t)now);
	text += device_id;
	Config & cfgs = Application::instance()->getConfig();

	std::string secret_key = cfgs.get_string("secret_key");
	std::string digest = mongo::md5simpledigest(text);
	text = boost::lexical_cast< std::string >((std::uint32_t)now) + "," + device_id + "," +digest;
	// base64

	std::string code;
	Base64::Encode(text,&code);

	// encode to json-response
	Json::Value json = defaultResponseJsonData();
	json["result"] = code ;

	send_http_response(nc,json.toStyledString(),defaultResponseHeaders());
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
//void handle_innerdevice_login(struct mg_connection *nc, struct http_message *hm ){
//
//}

//修改防区密码
void HttpService::handle_seczone_passwd_set(struct mg_connection *nc, struct http_message *hm ){
	Json::Value data = defaultResponseJsonData();
	
	if( !check_auth(nc,hm)) {
        return;
    }

    std::string oldpwd,newpwd;
    newpwd = http_get_var(hm,"new");
    oldpwd = http_get_var(hm,"old");
    if(newpwd.length() == 0){
        send_http_response_error(nc,Error_ParameterInvalid);
        return;
    }

    if(!InnerController::instance()->seczone_guard().setPassword(oldpwd,newpwd)){
        send_http_response_error(nc,Error_ParameterInvalid);
        return;
    }


    send_http_response_okay(nc);
}

// 设置设备运行参数
// 修改密码，保存到 settings.user 文件
void HttpService::handle_params_set(struct mg_connection *nc, struct http_message *hm ){
    if( check_auth(nc,hm) == false){
        return;
    }
    PropertyStringMap values;
    std::vector< std::string > names = {"watchdog_enable","alarm_enable","reboot",
                                        "call_in_enable","save","seczone_mode","initdata",
                                        "propserver_url"
                                        };
    for( auto _ : names){
        std::string value = http_get_var(hm,_);
        if(value.length()){
            values[_] = value;
        }
    }
    InnerController::instance()->setParameterValues(values);
    send_http_response_okay(nc);
}

void HttpService::ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
	struct http_message *hm = (struct http_message *) ev_data;
	HttpService* http = HttpService::instance().get();
    std::string method ;
    std::string url;

    try {
        switch (ev) {
            case MG_EV_HTTP_REQUEST:
                method = std::string(hm->method.p,hm->method.len) ;
                boost::to_lower(method);
                url =std::string (hm->uri.p,hm->uri.len);
                Application::instance()->getLogger().debug(url);
                if (mg_vcmp(&hm->uri, "/smartbox/discover") == 0) {
//				http->handle_innerdevice_discover(nc, hm); /* Handle RESTful call */

                } else if (mg_vcmp(&hm->uri, "/smartbox/api/status") == 0) { // 设备状态查询
                    http->handle_status_query(nc, hm);
                } else if (mg_vcmp(&hm->uri, "/smartbox/api/params") == 0) { // 设备参数设置
                    http->handle_params_set(nc, hm);
                } else if (mg_vcmp(&hm->uri, "/smartbox/api/innerscreen/reqcode") == 0) {
                    // 请求新的注册码
                    http->handle_innerdevice_reqcode(nc, hm);

                } else if (mg_vcmp(&hm->uri, "/smartbox/api/innerscreen") == 0) {

                    if (method == std::string("post")) { // 注册
                        http->handle_innerdevice_register(nc, hm);
                    } else if (method == "delete") { // 注销
//                    http->handle_innerdevice_remove(nc, hm);
                    }
                } else if (mg_vcmp(&hm->uri, "/smartbox/api/seczone/passwd") == 0) { //防区密码设置
                    http->handle_seczone_passwd_set(nc, hm);

                } else if (mg_vcmp(&hm->uri, "/smartbox/api/seczone/params") == 0) { //防区参数设置

                    if (method == std::string("post")) { // 设置防区参数
                        http->handle_seczone_param_set(nc, hm);
                    } else if (method == "get") { //  查询防区设置参数
                        http->handle_seczone_param_query(nc, hm);
                    }
                } else if (mg_vcmp(&hm->uri, "/smartbox/api/seczone/onekey") == 0) { //一键设防
                    http->handle_seczone_onekey_set(nc, hm);

                } else if (mg_vcmp(&hm->uri, "/smartbox/api/seczone/emergency") == 0) { //报警撤销
                    if (method == std::string("delete")) {
                        http->handle_seczone_emergency_discard(nc, hm);
                    }
                } else if (mg_vcmp(&hm->uri, "/smartbox/api/seczone/mode") == 0) { // 设置安防模式的参数值
                    if (method == std::string("post")) {
                        http->handle_seczone_mode_param_set(nc, hm);
                    } else if (method == "get") {
                        http->handle_seczone_mode_param_query(nc, hm);
                    }
                } else if (mg_vcmp(&hm->uri, "/smartbox/api/seczone/history/list") == 0) { //查询安防设置记录
                    http->handle_seczone_history_query(nc, hm);

                } else if (mg_vcmp(&hm->uri, "/smartbox/api/emergency/opendoor") == 0) { //查询安防设置记录
                    if (method == "post") {
                        http->handle_emergency_opendoor(nc, hm);
                    }
                } else if (mg_vcmp(&hm->uri, "/smartbox/api/seczone/params/reset") == 0) { //重置防区参数
                    if (method == "post") {
                        http->handle_seczone_param_reset(nc, hm);
                    }
                } else if (mg_vcmp(&hm->uri, "/smartbox/api/opendoor/password") == 0) { //设置单元门开门密码
                    if (method == "post") {
                        http->handle_opendoor_password_set(nc, hm);
                    }
                } else if (mg_vcmp(&hm->uri, "/smartbox/api/opendoor/password/verify") == 0) { //单元机开门密码校验
                    if (method == "post") {
                        http->handle_opendoor_password_verify(nc, hm);
                    }
                } else {
                    mg_serve_http(nc, hm, s_http_server_opts); /* Serve static content */
                }
                break;
            default:
                break;
        }
    }catch (boost::bad_lexical_cast& e){
        send_http_response_error(nc,Error_ParameterInvalid);
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
    struct mg_connection *nc;
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
	Application::instance()->getLogger().info("starting http server on port :" + http_port);
	mg_set_protocol_http_websocket(nc);
	s_http_server_opts.enable_directory_listing = "yes";
	
	printf("Starting RESTful server on port %s, serving %s\n", http_port.c_str(),
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

// 向外 http 连接请求返回处理入口
void ev_connect_handler(struct mg_connection *nc, int ev, void *ev_data) {

    std::string name = (char*) nc->user_data;
    HttpService* http = HttpService::instance().get();

    if (ev == MG_EV_HTTP_REPLY) { // 服务器返回
        struct http_message *hm = (struct http_message *)ev_data;
        nc->flags |= MG_F_CLOSE_IMMEDIATELY;


        Json::Reader reader;
        Json::Value root;
        std::string json(hm->body.p,hm->body.len);
        Application::instance()->getLogger().debug("Http Response:" + json);
        if (reader.parse(json, root)){
            if( root.get("status",Json::Value("0")).asInt() != 0){
                // error
                return ;
            }
        }else{
            Application::instance()->getLogger().debug("Http Response Data Parse Error");
            return ;
        }

        InnerController::instance()->onHttpResult(name,root);

    } else if (ev == MG_EV_CLOSE) {
//        exit_flag = 1;
    };

}

// 设备上线查询运行参数
void HttpService::handle_resp_init_data(Json::Value& root ){





}

void HttpService::http_request(const std::string& url, const PropertyStringMap& headers, const PropertyStringMap& vars,
                  void* user_data){
    struct mg_connection *nc;

    //处理http header
    std::string head_text ;
    for(auto p:headers) {
        boost::format fmt("%s: %s\r\n");
        fmt % p.first.c_str() % p.second.c_str();
        head_text +=  fmt.str();
    }
    if(head_text.length() == 0){
        head_text = "Content-Type: application/x-www-form-urlencoded\r\n";
    }

    // 处理post 参数
    std::string var_text ;
    for(auto p:vars) {
        std::string key ,value;
        key = p.first;
        value = p.second;
        mg_str k = mg_mk_str(key.c_str());
        mg_str v = mg_mk_str(value.c_str());

        k = mg_url_encode(k);
        v = mg_url_encode(v);
        key = std::string(k.p,k.len);
        value = std::string(v.p,v.len);

        free((void*)k.p);
        free((void*)v.p);

        if( var_text.length()){
            var_text +=  "&";
        }
        var_text += key + "=" + value;
    }
    if( var_text.length()) {
        nc = mg_connect_http(&mgr, ev_connect_handler,url.c_str(), head_text.c_str(), var_text.c_str());
    }else{
        nc = mg_connect_http(&mgr, ev_connect_handler,url.c_str(), head_text.c_str(), NULL);
    }
    nc->user_data = user_data;

}

//
//void HttpService::ev_connect_handler(struct mg_connection *nc, int ev, void *ev_data) {
//    std::string name = (char*) nc->user_data;
//
//    if (ev == MG_EV_HTTP_REPLY) { // 服务器返回
//        struct http_message *hm = (struct http_message *)ev_data;
//        nc->flags |= MG_F_CLOSE_IMMEDIATELY;
//
//        if( name == "propserver/initdata"){
//
//        }
//
//    } else if (ev == MG_EV_CLOSE) {
////        exit_flag = 1;
//    };
//}



// 防区参数设置
void HttpService::handle_seczone_param_set(struct mg_connection *nc, struct http_message *hm ){
    if( check_auth(nc,hm) == false ){
        return;
    }
    std::string pwd ,port ,name,normalstate,triggertype,onekeyset,currentstate,nursetime,online,alltime ,delaytime;
    pwd = http_get_var(hm,"passwd");
    name = http_get_var(hm,"name");
    normalstate = http_get_var(hm,"normalstate");
    triggertype = http_get_var(hm,"triggertype");
//    onekeyset = http_get_var(hm,"onekeyset");
//    currentstate = http_get_var(hm,"currentstate");
    nursetime = http_get_var(hm,"nursetime");
    online = http_get_var(hm,"online");
    alltime = http_get_var(hm,"alltime");
    port = http_get_var(hm,"port");
    delaytime = http_get_var(hm,"delaytime");

    seczone_conf_t cfg ;
    cfg.port = boost::lexical_cast<int>(port);
    cfg.name = name;
    cfg.normalstate = boost::lexical_cast<int>(normalstate);
    cfg.triggertype = boost::lexical_cast<int>(triggertype);
//    cfg.onekeyset = onekeyset;
//    cfg.currentstate = currentstate;
    cfg.nursetime = boost::lexical_cast<int>(nursetime);
    cfg.online = boost::lexical_cast<int>(online);
    cfg.alltime = boost::lexical_cast<int>(alltime);
    cfg.delaytime = boost::lexical_cast<int>(delaytime);
    Application::instance()->getLogger().debug(" handle_seczone_param_set () , delaytime:" + delaytime);
    InnerController::instance()->seczone_guard().setSecZoneParams(cfg,pwd);

    send_http_response_okay(nc);

}

//查询防区设置参数
void HttpService::handle_seczone_param_query(struct mg_connection *nc, struct http_message *hm ){

    Json::Value value;
    value = InnerController::instance()->seczone_guard().getSecZoneParams();
    send_http_response_result(value,nc);
}

void HttpService::handle_seczone_onekey_set(struct mg_connection *nc, struct http_message *hm ){
    if(check_auth(nc,hm) == false){
        return;
    }
    std::string passwd, state;
    passwd = http_get_var(hm,"passwd");
    state = http_get_var(hm,"state");
    bool onoff = false;

    if( state == "on"){
       onoff = true;
    }

    InnerController::instance()->seczone_guard().onekeySet(passwd,onoff);

    send_http_response_okay(nc);
}

//撤销报警
void HttpService::handle_seczone_emergency_discard(struct mg_connection *nc, struct http_message *hm ){
    if( ! check_auth(nc,hm)){
        return ;
    }
    std::string passwd ;
    passwd = http_get_var(hm,"passwd");

    InnerController::instance()->seczone_guard().discardEmergency(passwd);

    send_http_response_okay(nc);
}

//设置指定设防模式的参数
void HttpService::handle_seczone_mode_param_set(struct mg_connection *nc, struct http_message *hm ){
    if( ! check_auth(nc,hm)){
        return ;
    }
    std::string passwd;
    int mask;
    int mode;
    passwd = http_get_var(hm,"passwd");
    try {
        mode =  boost::lexical_cast<int>( http_get_var(hm, "mode") );
        mask = boost::lexical_cast<int>( http_get_var(hm, "value") );
    }catch (boost::bad_lexical_cast& e ){

        send_http_response_error(nc,Error_ParameterInvalid,e.what());
        return;
    }
    InnerController::instance()->seczone_guard().setSecModeValue(mode,mask);
    send_http_response_okay(nc);
}

// 查询所有模式参数
void HttpService::handle_seczone_mode_param_query(struct mg_connection *nc, struct http_message *hm ){
    if( ! check_auth(nc,hm)){
        return ;
    }

    Json::Value value = InnerController::instance()->seczone_guard().getSecModeList();
    send_http_response_result(value,nc);
}

void HttpService::handle_seczone_history_query(struct mg_connection *nc, struct http_message *hm ){
// todo. 预备完成 历史设置记录查询
}

// 物业端发送业主房间开门指令
void HttpService::handle_emergency_opendoor(struct mg_connection *nc, struct http_message *hm ){
    if( !check_auth(nc,hm)){
        return ;
    }
    std::string secret_key ;
    secret_key = http_get_var(hm,"rand_key");
    InnerController::instance()->openDoor(secret_key);
    send_http_response_okay(nc);
}

//防区参数重设
void HttpService::handle_seczone_param_reset(struct mg_connection *nc, struct http_message *hm ){
    if( !check_auth(nc,hm)){
        return ;
    }
    InnerController::instance()->seczone_guard().resetParams();
    send_http_response_okay(nc);
}

//设置单元门开门密码
void HttpService::handle_opendoor_password_set(struct mg_connection *nc, struct http_message *hm ){
    std::string password ;
    password = http_get_var(hm,"password");
    InnerController::instance()->setOpenDoorPassword(password);
    send_http_response_okay(nc);
}

void HttpService::handle_opendoor_password_verify(struct mg_connection *nc, struct http_message *hm ){
    std::string password ;
    password = http_get_var(hm,"password");
    bool ret = InnerController::instance()->verifyOpenDoorPassword(password);

    // encode to json-response
    Json::Value json = defaultResponseJsonData();
    json["result"] = 0 ;
    if( !ret){
        json["result"] = 1;
    }
    send_http_response(nc,json.toStyledString(),defaultResponseHeaders());
}