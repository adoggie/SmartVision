//
// Created by bin zhang on 2018/12/1.
//

#include "request.h"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include "cJSON.h"
//#include "innerproc.h"
#include "jsonrpc-c.h"

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>
#include<sys/types.h>
#include<sys/socket.h>

#include "innerproc.h"
#include "app_client.h"

#define LINE_BUSY "line_is_busy"

int get_peer_address(int fd, char *peeraddress);
void send_call_accept_to_app(cJSON *root,char *raddress, int rport);
void send_linebusy_to_app(char *raddress, int rport);
void save_ipconf_of_outerbox(cJSON * root);
void save_ipconf_of_innerbox(cJSON *root);
void send_callaccept_from_otherinner_to_app(char *raddress, int rport);
void send_qrcode_opendoor_authcode_to_app(char *qrcodeinfo);

cJSON * parseMessage(char* data,int fd) {
    cJSON *root;
    char *end_ptr = NULL;

    if ((root = cJSON_Parse(data)) != NULL) {
        char * str_result = cJSON_Print(root);
        printf("== parseMessage() Received:\n%s\n", str_result);
        free(str_result);

        if (root->type == cJSON_Object) {
            //eval_request(server, conn, root);
            cJSON *result, *audio;
            result = cJSON_GetObjectItem(root, "result");
            if (result != NULL && result->type == cJSON_String) {
                char raddress[64];
                bzero(raddress, 64);
                int rport = get_peer_address(fd, raddress);

                if (strcmp(result->valuestring, "call_accept") == 0)
                    send_call_accept_to_app(root, raddress, rport);

                if (strcmp(result->valuestring, LINE_BUSY) == 0)
                    send_linebusy_to_app(raddress, rport);

                if (strcmp(result->valuestring, "ipconf_of_outerbox") == 0) {
                    save_ipconf_of_outerbox(root);
                }

                if (strcmp(result->valuestring, "ipconf_of_innerbox") == 0){
                    save_ipconf_of_innerbox(root);
                }


                if (strcmp(result->valuestring, "call_accept_from_otherinner") == 0)
                    send_callaccept_from_otherinner_to_app(raddress, rport);

                if (strcmp(result->valuestring, "qrcode_opendoor_authcode") == 0) {
                    cJSON *value = cJSON_GetObjectItem(root, "value");
                    if ((value != NULL) && (value->type == cJSON_String))
                        send_qrcode_opendoor_authcode_to_app(value->valuestring);
                }

            }
        }
    }
    return root;
}

//int send_recv_msg(const char* dest, unsigned short port ,const char* msg,size_t size,char** retmsg,size_t * retsize ){
int send_recv_msg(const char* dest, unsigned short port ,const char* msg,size_t size){

    struct sockaddr_in addr;
    int sock = 0;

    int addr_len = sizeof(struct sockaddr_in);
    if((sock = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("udp socket");
        return -1;
    }
    int ret;

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(dest);

//    fcntl(sockfd,F_SETFL,fcntl(sockfd,F_GETFL,0)|O_NONBLOCK);
    ret = connect(sock, (struct sockaddr*)&addr,sizeof(struct sockaddr));
    if(ret){
        close(sock);
        return -1;
    }
    ret = send(sock,msg,size,0);
    if( ret == -1){
        close(sock);
        return -1;
    }
    char * buffer =  NULL ;
    int buffer_size = 0;

    char data [2000];
    fd_set set;
    struct timeval tm;
    while(1){

        tm.tv_sec = 15;
        tm.tv_usec = 0;
        FD_ZERO(&set);
        FD_SET(sock, &set);
        if(select(sock+1, NULL, &set, NULL, &tm) == 0) { // timeout
            printf("==send_recv_msg() read timeout.\n");
            break ;
        }
        ret = recv(sock,data,sizeof(data),0);
        if(ret <=0 ){
            //
            break;
        }
        if(!buffer){
            buffer = malloc(ret);
            memcpy(buffer,data,ret);
            buffer_size = ret ;

        }else{
            char *temp = buffer;
            buffer = malloc(ret+buffer_size);
            memcpy(buffer,temp,buffer_size);
            memcpy(buffer+buffer_size,data,ret);
            buffer_size += ret ;
            free(temp);
        }
        char * packbuffer = malloc(buffer_size+1);
        packbuffer[buffer_size] ='\0';
        memcpy(packbuffer,buffer,buffer_size);
        cJSON * root = NULL;
         root = parseMessage(packbuffer,sock);
        free(packbuffer);
        if(root){
            cJSON_Delete(root); // 2018.12.14
            break ;
        }

    }

    close(sock);
    if(buffer){
        free(buffer);
    }

    printf(" == send_recv_msg()  finished \n");

    return 0;
}


void thread_sleep(seconds) {
    struct timeval t_timeval;
    t_timeval.tv_sec = (seconds );
    t_timeval.tv_usec = (seconds % 1000);
    select(0, NULL, NULL, NULL, (struct timeval*)&t_timeval);
}


void  mutex_lock(pthread_mutex_t* mutex){
    pthread_mutex_lock(mutex);
}

void  mutex_unlock(pthread_mutex_t* mutex){
    pthread_mutex_unlock(mutex);
}




typedef  struct server_list_mgr{
    struct jrpc_server * servers[10];
    int server_num;
    fd_set accept_fds;
    int max_fd;
} ServerListMgr;

static ServerListMgr servermgr;


int server_init(struct jrpc_server *server,int port){
    if(server ==NULL){
        servermgr.server_num = 0;
        memset(&servermgr.servers,0, sizeof(servermgr.servers));
        servermgr.max_fd = 0;
        FD_ZERO(&servermgr.accept_fds);
        return 0 ;
    }
    server->port_number = port;
    server->sock = 0;

    server->sock = socket(AF_INET,SOCK_STREAM,0);
    if(server->sock<0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int yes = 1;
    if (setsockopt(server->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    bzero(&server_addr,sizeof(struct sockaddr_in));

    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    server_addr.sin_port=htons(server->port_number);

    if (bind(server->sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in) )<0){
        perror("bind failed");
        exit(EXIT_FAILURE);
    }


    if (listen(server->sock, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listener on port %d \n",server->port_number);
    FD_SET(server->sock, &servermgr.accept_fds);
    servermgr.servers[servermgr.server_num] = server;
    servermgr.server_num++;

    if(server->sock > servermgr.max_fd){
        servermgr.max_fd = server->sock;
    }

    return 0;
}


#define  MaxReadBufferSize 1024

static int _send_response(struct jrpc_connection * conn, char *response) {
    int fd = conn->fd;
    char *tmpstr = malloc(strlen(response)+2);
    memset(tmpstr,0,strlen(response)+2);

    printf("== send_response() start %d\n",time(NULL));
//	if (conn->debug_level > 1)
    printf("JSON Response:\n%s\n", response);
    sprintf(tmpstr,"%s^",response);
    write(fd, tmpstr, strlen(tmpstr));
    printf("== send_response() end %d\n",time(NULL));
    free(tmpstr);
    return 0;
}

static int _send_error(struct jrpc_connection * conn, int code, char* message,
                      cJSON * id) {
    int return_value = 0;
    cJSON *result_root = cJSON_CreateObject();
    cJSON *error_root = cJSON_CreateObject();
    cJSON_AddNumberToObject(error_root, "code", code);
    cJSON_AddStringToObject(error_root, "message", message);
    cJSON_AddItemToObject(result_root, "error", error_root);
    cJSON_AddItemToObject(result_root, "id", id);
    char * str_result = cJSON_Print(result_root);
    return_value = _send_response(conn, str_result);
    free(str_result);
    cJSON_Delete(result_root);
    free(message);
    return return_value;
}

static int _send_result(struct jrpc_connection * conn, cJSON * result,
                       cJSON * id) {
    int return_value = 0;
    if(!result)
        return 0;
    cJSON *result_root = cJSON_CreateObject();
    if (result)
        cJSON_AddItemToObject(result_root, "result", result);
    cJSON_AddItemToObject(result_root, "id", id);

    char * str_result = cJSON_Print(result_root);
    return_value = _send_response(conn, str_result);
    free(str_result);
    cJSON_Delete(result_root);
    return return_value;
}


static int _invoke_procedure(struct jrpc_server *server,
                            struct jrpc_connection * conn, char *name, cJSON *params, cJSON *id) {
    cJSON *returned = NULL;
    int procedure_found = 0;
    jrpc_context ctx;
    ctx.error_code = 0;
    ctx.error_message = NULL;
    int i = server->procedure_count;
    while (i--) {
        if (!strcmp(server->procedures[i].name, name)) {
            procedure_found = 1;
            //ctx.data = server->procedures[i].data;
            //if(!strcmp(name,"door_open_state"))
            ctx.data = conn;
            returned = server->procedures[i].function(&ctx, params, id);
            break;
        }
    }
    if (!procedure_found)
        return _send_error(conn, JRPC_METHOD_NOT_FOUND,
                          strdup("Method not found."), id);
    else {

        if (ctx.error_code)
            return _send_error(conn, ctx.error_code, ctx.error_message, id);
        else
        {
            return _send_result(conn, returned, id);
        }
    }
}

static int _eval_request(struct jrpc_server *server,
                        struct jrpc_connection * conn, cJSON *root) {
    cJSON *method, *params, *id;
    cJSON *result;
    result = cJSON_GetObjectItem(root,"result");
    if (result != NULL && result->type == cJSON_String) {
        cJSON *audio = cJSON_GetObjectItem(root,"audio");
        if(audio !=NULL && audio->type == cJSON_String && strncmp(result->valuestring,"doorinfoget",11) == 0)
        {
            printf("audio info: %s\n",audio->valuestring);
        }
        else
        {
            printf("door result:%s\n",result->valuestring);
            _send_error(conn, JRPC_INVALID_REQUEST,
                       strdup("Audio info error"), NULL);
            return -1;
        }
        return 0;

    }

    method = cJSON_GetObjectItem(root, "method");
    if (method != NULL && method->type == cJSON_String) {
        params = cJSON_GetObjectItem(root, "params");
        if (params == NULL|| params->type == cJSON_Array
            || params->type == cJSON_Object) {
            id = cJSON_GetObjectItem(root, "id");
            if (id == NULL|| id->type == cJSON_String
                || id->type == cJSON_Number) {
                //We have to copy ID because using it on the reply and deleting the response Object will also delete ID
                cJSON * id_copy = NULL;
                if (id != NULL)
                    id_copy =
                            (id->type == cJSON_String) ? cJSON_CreateString(
                                    id->valuestring) :
                            cJSON_CreateNumber(id->valueint);
//				if (server->debug_level)
                printf("Method Invoked: %s\n", method->valuestring);
                //ztf for test interface not the result
/*
				if(strcmp(method->valuestring,"opendoor") == 0 && server->port_number == IN_PORT)
				{
					printf("OpenDoor CMD GET!\n");
					return 0;
				}
*/
                return _invoke_procedure(server, conn, method->valuestring,
                                        params, id_copy);
            }
        }
    }
    _send_error(conn, JRPC_INVALID_REQUEST,
               strdup("The JSON sent is not a valid Request object."), NULL);
    return -1;
}

static void _close_connection(struct jrpc_connection * conn) {
//    ev_io_stop(loop, w);
    on_event_socket_disconnected(conn->fd);
    close( conn->fd);
    conn->fd = -1;

//    if (conn->buffer){
//        free(conn->buffer);
//    }
//    free(conn);
}

void _destroy_connection(struct jrpc_connection * conn) {
    if (conn->buffer){
        free(conn->buffer);
    }
    free(conn);

}


void _on_json_message_error(struct jrpc_connection *conn,char * msg){

    printf("INVALID JSON Received:\n---\n%s\n---\n", msg);

    _send_error(conn, JRPC_PARSE_ERROR, strdup("Parse error. Invalid JSON was received by the server."), NULL);
    return _close_connection(conn);
}


void _get_json_message(struct jrpc_server *server,  struct jrpc_connection *conn,char * msg){


    cJSON * root;
    char * end_ptr;
    printf("parse json message: %s \n",msg);
    if ((root = cJSON_Parse_Stream(msg, &end_ptr)) != NULL) {
        printf("cjson decode succ..\n");
        if (server->debug_level > 1) {
            char * str_result = cJSON_Print(root);
            printf("=== Valid JSON Received:\n%s\n", str_result);
            free(str_result);
        }

        if (root->type == cJSON_Object) {
            _eval_request(server, conn, root);

        }

        cJSON_Delete(root);

    }
    else {
        _on_json_message_error(conn,msg);
    }
}

int message_decode(struct jrpc_connection *conn) {
    struct jrpc_server *server = conn->server;
    size_t bytes_read = 0;

    if (conn->fd == -1){
        return -1;
    }

    int fd = conn->fd;

    fd_set set;
    struct timeval tm;

    tm.tv_sec = 60;
    tm.tv_usec = 0;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    if(select(fd+1, NULL, &set, NULL, &tm) == 0) { // timeout
        printf("==message_decode() read timeout.\n");
        _close_connection(conn);
        return -1;
    }

    char *read_buf = malloc(MaxReadBufferSize+1);
    int ret = 0;
    memset(read_buf,0,MaxReadBufferSize+1);

    ret = read(fd,read_buf,MaxReadBufferSize);
    if(ret == -1){
        perror("read failed.");
        free(read_buf);
        _close_connection(conn);
        return -1;
    }
    if(ret == 0){
        free(read_buf);
//        if (server->debug_level)
//            printf("Client closed connection.\n");
        _close_connection(conn);
        return -1;
    }
    printf("read data:%s\n",read_buf);

    char * parse_buf = NULL;

    if(conn->buffer){
        int buf_size = strlen(conn->buffer);
        int new_size = buf_size + ret + 1 ;
        parse_buf = malloc(new_size);

        memset(parse_buf,0,new_size);
        memcpy(parse_buf,conn->buffer,buf_size);
        memcpy(parse_buf + buf_size,read_buf,ret);
        free(conn->buffer);
        conn->buffer = NULL;

        free(read_buf);

    }else{
        parse_buf = read_buf;
    }
    // parse_buf  保存了当前最新的json数据缓存，可能包含多个json消息，还有不完整的json消息


    yajl_handle hand;
    yajl_status stat;
    yajl_gen_config conf = { 0, "  " };
    yajl_gen g;
    yajl_parser_config cfg = { 1, 1 };

//	g = yajl_gen_alloc(&conf, NULL); // 这个东西分配了产生内存泄露

//	hand = yajl_alloc(NULL, &cfg, NULL, (void *) g);
    hand = yajl_alloc(NULL, &cfg, NULL, NULL);

    char *cursor = parse_buf;
    stat = yajl_parse(hand, (const unsigned char*)cursor, strlen(cursor));

    int consumed = 0 ;
    while( stat == yajl_status_ok) {
        consumed = yajl_get_bytes_consumed(hand);

//		yajl_gen_clear(g);
        yajl_free(hand);

        if(consumed<=0 ){
            printf("------------------- consumed is 0 -----------\n");
            break;
        }
        char * json_msg = malloc( consumed + 1);
        json_msg[consumed] = '\0';
        memcpy(json_msg,cursor,consumed);
        _get_json_message(server,conn,json_msg);

        free(json_msg);

        cursor+=consumed;
//		g = yajl_gen_alloc(&conf, NULL);
//		hand = yajl_alloc(NULL, &cfg, NULL, (void *) g);
        hand = yajl_alloc(NULL, &cfg, NULL, NULL);
        stat = yajl_parse(hand, (const unsigned char*)cursor, strlen(cursor));
    }

//	yajl_gen_clear(g);
    yajl_free(hand);



    if(stat == yajl_status_insufficient_data ){
        conn->buffer = malloc(strlen(cursor)+1);
        conn->buffer[strlen(cursor)] = '\0';
        strcpy(conn->buffer,cursor);
    }

    if (stat != yajl_status_ok && stat != yajl_status_insufficient_data){

        _on_json_message_error(conn,cursor);
    }

    free(parse_buf);
    puts("==message_decode() end \n");
    return 0;
}


void * connection_handler(void* p){
    pthread_detach(pthread_self());

    struct jrpc_connection *conn =(struct jrpc_connection *)p ;
    int ret ;
    puts("==connection_handler() \n");
    while(1){
        ret = message_decode(conn);
        if(ret == -1){
            break;
        }
    }

//    _close_connection(conn);
    _destroy_connection(conn);
    printf("== socket client thread exiting..\n");

}

int server_run(){
    int n;
    struct jrpc_server * server = NULL;
    for(n=0;n< sizeof(servermgr.servers)/ sizeof(struct jrpc_server *);n++){
        server = servermgr.servers[n];
        if(server ==  NULL){
            continue;
        }
    }
    int activity = 0;
    while(1){
        puts("== ready for select..\n");
        FD_ZERO(&servermgr.accept_fds);

        for(n=0;n< sizeof(servermgr.servers)/ sizeof(struct jrpc_server *);n++) {
            server = servermgr.servers[n];
            if (server == NULL) {
                continue;
            }
            FD_SET(server->sock,&servermgr.accept_fds);
        }
        activity = select( servermgr.max_fd + 1 , &servermgr.accept_fds , NULL , NULL , NULL);
        if ((activity < 0) ) {
            perror("select error");
//            exit(EXIT_FAILURE);
            break;
        }

        for(n=0;n< sizeof(servermgr.servers)/ sizeof(struct jrpc_server *);n++){
            server = servermgr.servers[n];
            if(server ==  NULL){
                continue;
            }
            if (FD_ISSET(server->sock, &servermgr.accept_fds)){
                int new_socket = 0;
                struct sockaddr_in address;
                size_t addrlen = sizeof(address);
                if ((new_socket = accept(server->sock, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                printf("== socket client come in on:%d\n",server->port_number);

                pthread_t thread_id;
                struct jrpc_connection *conn = malloc(sizeof(struct jrpc_connection));
                memset(conn,0,sizeof(struct jrpc_connection));
                conn->server = server;
                conn->fd = new_socket;
                if( pthread_create( &thread_id , NULL ,  connection_handler , (void*) conn) < 0) {
                    perror("could not create thread\n");
                    close(conn->fd);
                    free(conn);

//                    return 1;
                }
//                pthread_detach(thread_id);

            }
        }
    }

    return 0;
}

int server_stop(){
    return 0;
}

int server_clean(){
    return 0;
}


extern basic_conf_t g_baseconf;

void execute_one_cmd(const char *cmd, char *result,size_t size)
{
    char buf_ps[1024];
    memset(result,0,size);
//    char ps[1024]={0};
    FILE *ptr;
//    strcpy(ps, cmd);
    if((ptr=popen(cmd, "r"))!=NULL) {
        if(fgets(buf_ps, 1024, ptr)!=NULL) {
            strcat(result, buf_ps);

        }
        pclose(ptr);
        ptr = NULL;
    }else {
        printf("popen error\n");
    }
//    printf(" cmd result:%s\n",result);

}

//系统观察
char* system_stat(){
    pid_t pid = getpid();
    time_t t = time(NULL);
//    printf("local:     %s", asctime(localtime(&t)));
    char * app_info_format="Pid = %d \n"
                           "Time = %s \n"
                           "App info = version:%s,innerinterfaceip:%s,outerboxip:%s,outerboxip_1:%s,gateouterboxip:%s,"
                       "propertyip:%s,streamserverip:%s,callcenterip:%s,alarmcenterip:%s,doorid:%s,"
                       "appaudiostreamurl:%s,outerboxvideostreamurl:%s,outerboxvideostreamurl_1:%s,gateouterboxvideostreamurl:%s\n\n"
                       "Free mem = %s"
                       "VmRSS = %s "
                       "Threads = %s"
                       "Fds = %s \n";

    char free_mem[1024];
    char vmrss[1024];
    char threads[1024];
    char fds[1024];
    execute_one_cmd("busybox free -m | grep Mem",free_mem,sizeof(free_mem));
    char cmd[1024];
    sprintf(cmd,"cat /proc/%d/status | grep VmRSS",pid);
    execute_one_cmd(cmd,vmrss,sizeof(vmrss));
    sprintf(cmd,"cat /proc/%d/status | grep Threads",pid);
    execute_one_cmd(cmd,threads,sizeof(threads));
    sprintf(cmd,"lsof -n | grep %d -c",pid);
    execute_one_cmd(cmd,fds,sizeof(fds));

    puts(asctime(localtime(&t)));

    int sz = snprintf(NULL, 0, app_info_format,
            pid, asctime(localtime(&t)),g_baseconf.version,g_baseconf.innerinterfaceip,g_baseconf.outerboxip,g_baseconf.outerboxip_1,g_baseconf.gateouterboxip,
                      g_baseconf.propertyip,g_baseconf.streamserverip,g_baseconf.callcenterip,g_baseconf.alarmcenterip,g_baseconf.doorid,
                      g_baseconf.appaudiostreamurl,g_baseconf.outerboxvideostreamurl,g_baseconf.outerboxvideostreamurl_1,g_baseconf.gateouterboxvideostreamurl,
                      free_mem,vmrss,threads,fds);
    printf("== need size:%d\n",sz);

    char * result = malloc(sz+1);
    if(result) {
        memset(result, 0, sz + 1);
        snprintf(result,sz,app_info_format,
                 pid, asctime(localtime(&t)),g_baseconf.version,g_baseconf.innerinterfaceip,g_baseconf.outerboxip,g_baseconf.outerboxip_1,g_baseconf.gateouterboxip,
                 g_baseconf.propertyip,g_baseconf.streamserverip,g_baseconf.callcenterip,g_baseconf.alarmcenterip,g_baseconf.doorid,
                 g_baseconf.appaudiostreamurl,g_baseconf.outerboxvideostreamurl,g_baseconf.outerboxvideostreamurl_1,g_baseconf.gateouterboxvideostreamurl,
                 free_mem,vmrss,threads,fds);
        return result;
    }
    return NULL;
}
