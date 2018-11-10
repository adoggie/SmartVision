/*
 * innerproc.c
 *
 *  Created on: 2016.11.14
 *      Author: ZTF
 */

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
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <linux/watchdog.h>

#ifdef ANDROID
#include "ifaddrs.h"//为兼容android系统

#ifdef YANGCHUANG
#include "ycapic.h"
#endif

#else
#include <ifaddrs.h>//原生Linux的库
#endif

#include "innerproc.h"
#include "jsonrpc-c.h"
#include "iniparser.h"
#include "rc4.h"

#define PORT 1234  // the port users will be connecting to

struct jrpc_server my_in_server;
struct jrpc_server my_out_server;
struct jrpc_server my_sec_server;
char * opencmd = "open the door please!\n";

#ifdef MAICHONG
#define PREFIX "/data/user"
#else
#define PREFIX "/etc"
#endif

#define INNERBOX_CONF PREFIX"/innerbox.conf"
#define BASIC_CONF PREFIX"/basic.conf"
#define SECZONE_CONF PREFIX"/seczone.conf"
#define INNER_DB PREFIX"/inner.db"
//#define CALLHISTORY_PATH "/var/www/html/callhisory/"
#define CALLHISTORY_PATH "/data/local/tmp/"
//#define MSGHISTORY_PATH "/var/www/html/msghisory/"

#ifdef ANDROID
#define MSGHISTORY_PATH "/data/local/tmp/"
#else
#define MSGHISTORY_PATH "/var/www/html/msghisory/"
#endif

#define ZONE_NUMBER 16

#define PAGE_NUMBER 10

#define PID_PATH "/etc/serverpid"

#define ACCESS_PERMISSION_DENIED "Permission Denied.Join Family First!"
#define WRONG_PARAMETER "Wrong Parameters!"

basic_conf_t g_baseconf;
char g_localaddress[64];
seczone_conf_t  g_seczone_conf[ZONE_NUMBER]; //max 16
int g_seczone_count = 0;	//实际的防区数量
app_client_t * g_app_client_list = NULL;
char g_seczone_passwd[512];

char g_stream_outerbox[1024];
//char g_stream_phoneapp[1024];
//char g_stream_apptoout[1024];
//char g_stream_serverurl[1024];  //公共服务stream server
char g_last_caller[64];
char g_call_from_innerbox[2048];

char g_callotherinner_doorid[32];
char g_callotherinner_cmdstr[2048];
char g_otherinnerip[128];

int g_outerbox_online = 0;

int g_line_is_busy = 0;
int g_property_socket = 0;

int g_onekey_set = 0;

char g_last_callin_address[64];

int g_last_nursetime = 0;

int g_ipconf_get_flag = 0;
int g_innerbox_get_flag = 0;
int g_innerbox_count = 0;
innerbox_info_t * g_innerbox_info_list;

int g_update_state = 0;

#ifdef YANGCHUANG
#define VERSION "1.0.%s.%d-yc-I"
#elif MAICHONG
#define VERSION "1.0.%s.%d-mc-I"
#elif ARM
#define VERSION "1.0.%s.%d-am-I"
#elif X86
#define VERSION "1.0.%s.%d-x86-I"
#endif

#define LOG 1

#define AUDIO_STREAM "rtmp://%s:1935/%s"

#ifdef SQL
#include <sqlite3.h>  
#endif

/**connect out**/
typedef struct connect_watcher
{
        ev_io eio;
        struct addrinfo *addr; /* addrinfo for this connection. */
        struct addrinfo *addr_base; /* The base of the addrinfo list. */
        int pos;
        unsigned int buffer_size;
        char * buffer;
	char * data;
} connect_watcher;

connect_watcher g_connectout_watcher;
//g_connectout_watcher;

connect_watcher * new_connector(EV_P_ char *addrstr,char *portstr);
void create_basic_conf_file(void);
void query_info_from_property(char *cmdstr,struct ev_loop *loop);
int send_msg_to_center(char * serveraddr,char * cmd);
int get_peer_address(int fd, char *peeraddress);
void init_stream_url();
static void create_worker(void *(*func)(), void *arg); 
void callhistory_log_to_db(char *time,char *person,char *event);
void get_time_now(char * datetime);
void get_local_address();
void * ipaddr_query_from_property();
int send_cmd_to_local(char * serveraddr,char * cmd);
static void create_worker(void *(*func)(), void *arg);

void travel_seczone_conf();

void get_rand_str(char s[],int number)
{
        char str[64] = "00123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        int i;
        char ss[2];
#ifdef ANDROID
        srand48((unsigned int)time((time_t *)NULL));
        for(i=1;i<=number;i++){
                sprintf(ss,"%c",str[(lrand48()%61)+1]);
                strcat(s,ss);
        }
#else
        srand((unsigned int)time((time_t *)NULL));
        for(i=1;i<=number;i++){
                sprintf(ss,"%c",str[(rand()%61)+1]);
                strcat(s,ss);
        }

#endif
}

void generate_audio_stream(char * ipstr,char *outstream,char *randstr)
{
	sprintf(outstream,AUDIO_STREAM,ipstr,randstr);
}

void update_app_line_state(app_client_t * appt,call_type_e type,int port,char *ipaddr,int socket) {

	appt->line_state->calltype = type;
	appt->line_state->peerport = port;
	appt->line_state->peersocketfd = socket;
	if(ipaddr != NULL)
		strcpy(appt->line_state->peeripaddr,ipaddr);
	else
 		bzero(appt->line_state->peeripaddr,sizeof(appt->line_state->peeripaddr));
		
	
}

int get_local_sockinfo(int fd)
{

        struct sockaddr_storage  localAddr;//连接的对端地址
        socklen_t localLen = sizeof(localAddr);
        int ret = 0;
        char ipstr[128];
        bzero(ipstr,128);
        int port =0;
        memset(&localAddr, 0, sizeof(localAddr));
        //get the app client addr
        ret = getsockname(fd, (struct sockaddr *)&localAddr, &localLen);
        if(ret == -1) perror("getsockname error!");
        if (localAddr.ss_family == AF_INET) {
                struct sockaddr_in *s = (struct sockaddr_in *)&localAddr;
                port = ntohs(s->sin_port);
                inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
        } else if(localAddr.ss_family == AF_INET6) {
                struct sockaddr_in6 *s = (struct sockaddr_in6 *)&localAddr;
                port = ntohs(s->sin6_port);
                inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
        }

        //strcpy(localaddress,ipstr);
        DEBUG_INFO("connected peer address = %s:%d",ipstr,port );
	return port;

/*
	struct sockaddr_in  localAddr;//local address
	int localLen = sizeof(localAddr);
#ifdef ANDROID_	
	char ipAddr[INET6_ADDRSTRLEN];
#else
	char ipAddr[INET_ADDRSTRLEN];
#endif
	memset(&localAddr, 0, sizeof(localAddr));
	getsockname(fd, (struct sockaddr *)&localAddr, &localLen);
#ifdef ANDROID_
	printf("local:%s:%d\n",inet_ntop(AF_INET6, &localAddr.sin_addr, ipAddr, sizeof(ipAddr)), ntohs(localAddr.sin_port));
#else
	printf("local:%s:%d\n",inet_ntop(AF_INET, &localAddr.sin_addr, ipAddr, sizeof(ipAddr)),ntohs(localAddr.sin_port));
#endif

	return ntohs(localAddr.sin_port);
*/
}


innerbox_info_t * malloc_innerbox_info()
{
        innerbox_info_t *newinnerbox = malloc(sizeof(innerbox_info_t));
        bzero(newinnerbox,sizeof(innerbox_info_t));
        newinnerbox->next = NULL;

        return newinnerbox;
}

void delete_innerbox_info_list()
{
        innerbox_info_t * loop = g_innerbox_info_list;
        innerbox_info_t * delit;
        while(loop) {
                delit = loop;
                loop = loop->next;
                free(delit);
        }
        g_innerbox_info_list = NULL;
}

innerbox_info_t * select_from_innerbox_info_list_byid(char *doorid)
{
        innerbox_info_t * loop = g_innerbox_info_list;
        while(loop) {
                if(strcmp(doorid,loop->doorid) == 0)
                        return loop;
                loop = loop->next;
        }
        return NULL;
}

innerbox_info_t * select_from_innerbox_info_list_byip(char *ipaddr)
{
        innerbox_info_t * loop = g_innerbox_info_list;
        while(loop) {
                if(strcmp(ipaddr,loop->ipaddr) == 0)
                        return loop;
                loop = loop->next;
        }
        return NULL;
}

void insert_into_innerbox_info_list(char* doorid,char *ipaddr)
{

	if(strlen(g_localaddress) == 0)
		get_local_address();
#ifdef ANDROID
		if(strcmp(ipaddr,g_localaddress) == 0)
			strcpy(g_baseconf.doorid,doorid);
#else
		if(strcmp(ipaddr,g_baseconf.outerinterfaceip) == 0)
			strcpy(g_baseconf.doorid,doorid);
#endif		

        innerbox_info_t * nbox = malloc_innerbox_info();
        strcpy(nbox->doorid,doorid);
        strcpy(nbox->ipaddr,ipaddr);
        innerbox_info_t * loop1 = g_innerbox_info_list;
        if(loop1 == NULL) {
                g_innerbox_info_list = nbox;
                return;
        }
        while(loop1) {
                if(loop1->next==NULL) {
                        loop1->next = nbox;
                        return;
                }
                loop1 = loop1->next;
        }
}


void create_innerbox_info_conf(void)
{
        FILE    *   ini ;
        innerbox_info_t * loop = g_innerbox_info_list;

        ini = fopen(INNERBOX_CONF, "w");
        fprintf(ini,
        "#\n"
        "# innerbox configure file\n"
        "#\n"
        "\n"
        "[doorcount]\n"
        "number = %d ;\n"
        "\n",g_innerbox_count);
        int i = 0;
        while(loop) {
                fprintf(ini,"[door%d]\n"
                        "doorid = %s ;\n"
                        "ipaddr = %s ;\n"
                        "\n",i,loop->doorid,loop->ipaddr);
                i++;
                loop = loop->next;
        }

        fclose(ini);
//      fflush(ini);

}

int read_innerbox_info_from_conf(void)
{
        dictionary  *   ini ;

        /* Some temporary variables to hold query results */
        int             b ;
        int             i,j ;
        double          d ;
        char        *   s ;
        char doorid[32];
        char ipaddr[64];
        char inistring[64];

        bzero(doorid,32);
        bzero(ipaddr,64);
        bzero(inistring,64);

        if(g_innerbox_info_list)
                return 0;
        ini = iniparser_load(INNERBOX_CONF);
        if (ini==NULL) {
                DEBUG_ERR_LOG("cannot parse file: %s", INNERBOX_CONF);
                return -1 ;
        }

        /* Get attributes */
	if(strlen(g_localaddress) == 0)
		get_local_address();
        DEBUG_INFO("innerbox count:");
        g_innerbox_count = iniparser_getint(ini, "doorcount:number",1);
        DEBUG_INFO("count: [%d]", g_innerbox_count);
        for (j = 0;j<g_innerbox_count;j++) {
                bzero(doorid,32);
                bzero(ipaddr,64);
                bzero(inistring,64);
                snprintf(inistring,64,"door%d:doorid",j);
                s = iniparser_getstring(ini, inistring, NULL);
                if(s)
                        strncpy(doorid,s,32);
                bzero(inistring,64);
                snprintf(inistring,64,"door%d:ipaddr",j);
                s = iniparser_getstring(ini, inistring, NULL);
                if(s)
                        strncpy(ipaddr,s,64);
                insert_into_innerbox_info_list(doorid,ipaddr);
#ifdef ANDROID
		if(strcmp(ipaddr,g_localaddress) == 0)
			strcpy(g_baseconf.doorid,doorid);
#else
		if(strcmp(ipaddr,g_baseconf.outerinterfaceip) == 0)
			strcpy(g_baseconf.doorid,doorid);
#endif		
        }
 	iniparser_freedict(ini);
        return 0 ;
}

void travel_innerbox_infolist()
{
        innerbox_info_t * loop =  g_innerbox_info_list;

        while(loop) {
                DEBUG_INFO("doorid:%s,ipaddr:%s",loop->doorid,loop->ipaddr);
                loop  = loop->next;
        }
}




int
set_nonblocking(int fd)
{
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1)
                return -1;
        flags |= O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags) == -1)
                return -1;
        return 0;
}

static void init_callotherinner_stream_url(char * streamfrom,char * fromdoorid, char * streamto,char * todoorid)
{

#ifdef ANDROID
	sprintf(streamfrom,"rtmp://%s:1935/hls/callinner_%s",g_baseconf.streamserverip,fromdoorid);
	sprintf(streamto,"rtmp://%s:1935/hls/callinner_%s",g_baseconf.streamserverip,todoorid);
#else //not implement
	//sprintf(streamfrom,"rtmp://%s:1938/callinner_%s",g_localaddress,fromdoorid);
	//sprintf(streamto,"rtmp://%s:1938/callinner_%s",g_localaddress,todoorid);
	sprintf(streamfrom,"rtmp://192.168.0.2:1935/hls/callinner_%s",fromdoorid);
	sprintf(streamto,"rtmp://192.168.0.2:1935/hls/callinner_%s",todoorid);
#endif
}

//void * do_call_other_innerbox(char * otherinnerip)
void * do_call_other_innerbox(app_client_t * appt)
{

	pthread_detach(pthread_self());	

	//char cmdstr[2048];	
	char portstr[16];
	bzero(portstr,16);
	snprintf(portstr,16,"%d",OUT_PORT);

	char streamfrom[512];
	bzero(streamfrom,512);
	char streamto[512];
	bzero(streamto,512);
	
	
	struct ev_loop *loopinthread =ev_loop_new(0);
	connect_watcher *c = new_connector(loopinthread,appt->calling_otherinner_ip, portstr);
	if(!c) {
		DEBUG_ERR_LOG("Cannot connect to property server");
		char tmpstr[512];
		bzero(tmpstr,512);
		snprintf(tmpstr,512,"{\"result\":\"connot_connect_to_otherinner_%s\"}",appt->calling_otherinner_ip);			
		write(appt->socket_fd,tmpstr,sizeof(tmpstr));
		return NULL;
	}

	//memset(&g_connectout_watcher,0,sizeof(struct connect_watcher));
	c->buffer_size = 1500;
	if(!c->buffer)
		c->buffer = malloc(1500);
	memset(c->buffer, 0, 1500);
	c->pos = 0;
/*
	bzero(cmdstr,2048);
	snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_outerbox\",\"params\":{\"ipaddr\":\"%s\"}}",g_localaddress);
*/
	//write command to property 
// 	send(c->eio.fd,cmdstr,strlen(cmdstr),0);
	//set_nonblocking(c->eio.fd);
	//strcpy(g_baseconf.doorid,"202");
	init_callotherinner_stream_url(streamfrom,g_baseconf.doorid,streamto,appt->calling_otherdoorid);
	snprintf(g_callotherinner_cmdstr,1024,"{\"method\":\"call_from_otherinner\",\"params\":{\"doorid\":\"%s\",\"stream_url_from_otherinner\":\"%s\",\"stream_url_to_otherinner\":\"%s\"}}",g_baseconf.doorid,streamfrom,streamto);
	c->data = g_callotherinner_cmdstr;	
	c->eio.data = c;
 
	if (c) {
		DEBUG_INFO( 
		"Trying connection to %s:%d...",appt->calling_otherinner_ip,OUT_PORT
		);
		appt->current_state = CONNECTING;
		update_app_line_state(appt,APP2APP,OUT_PORT,appt->calling_otherinner_ip,c->eio.fd);

		ev_io_start(loopinthread, &c->eio);
		ev_run(loopinthread, 0);
	}
	else {
	       	DEBUG_ERR_LOG( "Can't create a new connection ");
		return NULL;
	}

}

static void connect_cb (EV_P_ ev_io *w, int revents)
{
        int remote_fd =  w->fd;
	connect_watcher * watcher = (connect_watcher *)(w->data);
        char *line = watcher->data;
        int len = strlen(line);
        size_t bytes_read = 0;

        if (revents & EV_WRITE)
        {

                DEBUG_INFO("remote ready for writing...");

                if (-1 == send(remote_fd,line , len, 0)) {
                	ev_io_stop(EV_A_ w);
			close(remote_fd);
			ev_loop_destroy(loop);
                      //perror("echo send");
                      DEBUG_INFO("echo send:%s",line);
		      if(errno == EAGAIN || errno == EBADF)
			{
				int pid = getpid();
                                char killmy[64];
                                snprintf(killmy,64,"kill -9 %d",pid);
                            //    system(killmy);
			}
                      return ;
                }
                    // data to send
	
                ev_io_stop(EV_A_ w);
                ev_io_set(EV_A_ w, remote_fd, EV_READ);
                ev_io_start(EV_A_ w);
	
        }
        else 

        if (revents & EV_READ)
        {
/*              int n;
                char str[100] = ".\0";

                printf("[r,remote]");
                n = recv(remote_fd, str, 100, 0);
                if (n <= 0) {
                        if (0 == n) {
                                
                                puts("orderly disconnect");
                                ev_io_stop(EV_A_ w);
                                close(remote_fd);
                        }  else if (EAGAIN == errno) {
                                puts("should never get in this state with libev");
                        } else {
                                close(remote_fd);
                                perror("recv");
                        }
                      return;
                }
                printf("socket client said: %s", str);
                close(remote_fd);
*/


               int fd = w->fd;
                if (watcher->pos == (watcher->buffer_size - 1)) {
                        char * new_buffer = realloc(watcher->buffer, watcher->buffer_size *= 2);
                        if (new_buffer == NULL) {
                                DEBUG_ERR_LOG("Memory error");
                                return;
//                              return close_connection(loop, w);
                        }
                        watcher->buffer = new_buffer;
                        memset(watcher->buffer + watcher->pos, 0, watcher->buffer_size - watcher->pos);
                }
                //send_response(conn,"test");
                // can not fill the entire buffer, string must be NULL terminated
                int max_read_size = watcher->buffer_size - watcher->pos - 1;
                if ((bytes_read = read(fd, watcher->buffer + watcher->pos, max_read_size))
                                == -1) {
                        ev_io_stop(EV_A_ w);
                        close(fd);
			if(errno == EAGAIN || errno == EBADF)
			{
                                //snprintf(killmy,64,"kill -9 %d",pid);
                                //system(killmy);
			}
                        //ev_io_stop(EV_A_ w);
                        //close(fd);
                        return;
                        //return close_connection(loop, w);
                }
                if (!bytes_read) {
                        // client closed the sending half of the connection
                        if (1)
                                DEBUG_INFO("Client closed connection.");
                        ev_io_stop(EV_A_ w);
                        close(fd);
                        return;
                        //return close_connection(loop, w);
                } else {
                        cJSON *root;
                        char *end_ptr = NULL;
                        watcher->pos += bytes_read;

                        if ((root = cJSON_Parse_Stream(watcher->buffer, &end_ptr)) != NULL) {
                                if ( 1) {
                                        char * str_result = cJSON_Print(root);
                                        DEBUG_INFO("Valid JSON Received:\n%s\n", str_result);
                                        free(str_result);
                                }

                                if (root->type == cJSON_Object) {
                                        //eval_request(server, conn, root);
					cJSON * result, *audio;
                                        result = cJSON_GetObjectItem(root, "result");
                                        if(result != NULL)
                                                if(result->type == cJSON_String) {
                                                        if(strcmp(result->valuestring,"call_accept") == 0) {
                                                                audio = cJSON_GetObjectItem(root,"audio");
                                                                if( audio != NULL)
                                                                        if(audio->type == cJSON_String) {

										app_client_t * appt = g_app_client_list;
									        while(appt) {
											
                									if(appt->line_state->calltype == APP2PROPERTY && appt->current_state == CONNECTING && appt->socket_fd > 0 && appt->calling_property == appt->socket_fd) {
											
												cJSON *result = cJSON_CreateObject();
												cJSON *content = cJSON_CreateObject();
												#ifdef ANDROID	
												cJSON_AddStringToObject(content,"audio_from_property",audio->valuestring);
												#else
												char rtmpstr[1024];
												bzero(rtmpstr,1024);
												if(strlen(g_localaddress) == 0)
													get_local_address();
												snprintf(rtmpstr,1024,"rtmp://%s:1936/hls/stream",g_localaddress);
												cJSON_AddStringToObject(content,"audio_from_property",rtmpstr);
												#endif
												cJSON_AddStringToObject(content,"audio_to_property",appt->audiostream_url);
												cJSON_AddItemToObject(result,"callout_params",content);
												cJSON *sendout = cJSON_CreateObject();
												cJSON_AddItemToObject(sendout,"result",result);
												char *str_send = cJSON_Print(sendout);
												char str_send0[1024];
												bzero(str_send0,1024);
												snprintf(str_send0,1024,"%s^",str_send);
												int ret = write(appt->socket_fd,str_send0,strlen(str_send0));
												free(str_send);
												appt->current_state = CALLING;	
												if(ret == -1)
													appt->current_state = OFFLINE;

							                                        char timestr[128];
                                                                                                bzero(timestr,128);
                                                                                                char event[128];
                                                                                                bzero(event,128);
                                                                                                get_time_now(timestr);
                                                                                                strcpy(event,"物业接听");
                                                                                                //callhistory_log_to_db(timestr,g_baseconf.propertyip,event);
                                                                                                callhistory_log_to_db(timestr,appt->line_state->peeripaddr,event);

                       										break;
                									}


                									appt = appt->next;
        									}

                                                                        }
                                                        }

                                                        if(strcmp(result->valuestring,"line_is_busy") == 0) {
								char *busystr = "{\"result\":\"line_is_busy\"}^";
								app_client_t * appt = g_app_client_list;
								while(appt) {
                							if(appt->socket_fd > 0 && appt->calling_property == appt->socket_fd) {
										write(appt->socket_fd,busystr,strlen(busystr));	
										bzero(appt->line_state,sizeof(call_line_state_t));
										appt->calling_property = 0;
										appt->current_state = IDLE;				
                       								break;
                							}

									if(appt->socket_fd > 0 && appt->calling_otherinner == appt->socket_fd) {
										if(appt->current_state == CONNECTING) {
											write(appt->socket_fd,busystr,strlen(busystr));	
											bzero(appt->line_state,sizeof(call_line_state_t));
											appt->calling_otherinner = 0;				
											appt->current_state = IDLE;	
										}			
                       								break;
                							}

                							appt = appt->next;
								}
                                                        }
							
							if(strcmp(result->valuestring,"ipconf_of_outerbox") == 0) {
								cJSON *ipaddr = cJSON_GetObjectItem(root,"outerbox_f1");
								
								if(ipaddr != NULL && ipaddr->type == cJSON_String) {
									strcpy(g_baseconf.outerboxip,ipaddr->valuestring);
								}
								ipaddr = cJSON_GetObjectItem(root,"gateouterbox");
								
								if(ipaddr != NULL && ipaddr->type == cJSON_String) {
									strcpy(g_baseconf.gateouterboxip,ipaddr->valuestring);
								}
								ipaddr = cJSON_GetObjectItem(root,"outerbox_x1");
								
								if(ipaddr != NULL && ipaddr->type == cJSON_String) {
									strcpy(g_baseconf.outerboxip_1,ipaddr->valuestring);
								}
								
								ipaddr = cJSON_GetObjectItem(root,"streamserver");
								
								if(ipaddr != NULL && ipaddr->type == cJSON_String) {
									strcpy(g_baseconf.streamserverip,ipaddr->valuestring);
								}

								g_ipconf_get_flag = 1;
								create_basic_conf_file();
							}
/*
							if(strcmp(result->valuestring,"ipconf_of_otherinner") == 0) {
								cJSON *ipaddr = cJSON_GetObjectItem(root,"ipaddr");
								
								if(ipaddr != NULL && ipaddr->type == cJSON_String) {
									//do call other innerbox 
									//...
									//not immplement
									printf("calling other:%s\n",ipaddr->valuestring);
									strcpy(g_otherinnerip,ipaddr->valuestring);
								}
								else
									goto out;
								cJSON *doorid = cJSON_GetObjectItem(root,"doorid");
								if(doorid != NULL && doorid->type == cJSON_String) {
									printf("calling from :%s\n",doorid->valuestring);
									strcpy(g_baseconf.doorid,doorid->valuestring);
								}
								else
									goto out;

								char methodstr[1024];
								bzero(methodstr,1024);
								char streamfrom[512];
								bzero(streamfrom,512);
								char streamto[512];
								bzero(streamto,512);
								init_callotherinner_stream_url(streamfrom,g_baseconf.doorid,streamto,g_callotherinner_doorid);
								snprintf(g_callotherinner_cmdstr,1024,"{\"method\":\"call_from_otherinner\",\"params\":{\"doorid\":\"%s\",\"stream_url_from_otherinner\":\"%s\",\"stream_url_to_otherinner\":\"%s\"}}",doorid->valuestring,streamfrom,streamto);
								
								
								create_worker(do_call_other_innerbox,0);

							}

*/
                                                        if(strcmp(result->valuestring,"ipconf_of_innerbox") == 0) {
                                                                cJSON *ipaddrarray = cJSON_GetObjectItem(root,"values");
                                                                cJSON *item,*doorid,*ipaddr;

                                                                if(ipaddrarray != NULL && ipaddrarray->type == cJSON_Array) {
                                                                        g_innerbox_count = cJSON_GetArraySize(ipaddrarray);
                                                                        g_innerbox_get_flag = g_innerbox_count;
                                                                        int i = 0;
                                                                        delete_innerbox_info_list();
                                                                        while(i < g_innerbox_count) {
                                                                                item = cJSON_GetArrayItem(ipaddrarray,i);
                                                                                if(item != NULL && item->type == cJSON_Object) {
                                                                                        doorid = cJSON_GetObjectItem(item,"doorid");
                                                                                        ipaddr = cJSON_GetObjectItem(item,"ipaddr");
                                                                                        if((doorid != NULL && doorid->type == cJSON_String) && (ipaddr != NULL && ipaddr->type == cJSON_String))
                                                                                                insert_into_innerbox_info_list(doorid->valuestring,ipaddr->valuestring);
                                                                                }
                                                                                i++;
                                                                        }
                                                                        create_innerbox_info_conf();
                                                                        //travel_innerbox_infolist();
                                                                }
                                                        }

							if(strcmp(result->valuestring,"call_accept_from_otherinner") == 0) {
								char resultstr[1024];
								bzero(resultstr,1024);
								char streamfrom[512];
								bzero(streamfrom,512);
								char streamto[512];
								bzero(streamto,512);
								int ret = 0;

								init_callotherinner_stream_url(streamfrom,g_baseconf.doorid,streamto,g_callotherinner_doorid);
								snprintf(resultstr,1024,"{\"result\":\"call_accept_from_otherinner\",\"params\":{\"stream_url_from_otherinner\":\"%s\",\"stream_url_to_otherinner\":\"%s\"}}^",streamto,streamfrom);

								app_client_t * appt = g_app_client_list;
								while(appt) {
									char raddress[128];
									bzero(raddress,128);
									int rport = get_peer_address(appt->socket_fd,raddress);
									if(rport != appt->port || strcmp(appt->ip_addr_str,raddress) != 0) {
										appt->current_state = OFFLINE;
										appt = appt->next;
										continue;
									}

									if(appt->socket_fd > 0 && appt->socket_fd == appt->calling_otherinner) {
										appt->current_state = CALLING;
										ret = write(appt->calling_otherinner,resultstr,strlen(resultstr));
									}

									if(ret == -1) {
										appt->current_state = OFFLINE;
										update_app_line_state(appt,IDLE,0,NULL,0);
									}

									appt = appt->next;	
								}
							}

                                                }
                                }
out:
                               //shift processed request, discarding it
                                memmove(watcher->buffer, end_ptr, strlen(end_ptr) + 2);

                                watcher->pos = strlen(end_ptr);
                                memset(watcher->buffer + watcher->pos, 0,
                                                watcher->buffer_size - watcher->pos - 1);

                                cJSON_Delete(root);

				ev_io_stop(EV_A_ w);
                                close(fd);

                        } else {
                                DEBUG_INFO("recv:%s",watcher->buffer);
                                // did we parse the all buffer? If so, just wait for more.
                                // else there was an error before the buffer's end
				if (end_ptr != (watcher->buffer + watcher->pos)) {
                                        if (1) {
                                                DEBUG_ERR_LOG("INVALID JSON Received:\n---\n%s\n---\n",
                                                                watcher->buffer);
                                        }
                                        /*
                                        send_error(conn, JRPC_PARSE_ERROR,
                                                        strdup(
                                                                        "Parse error. Invalid JSON was received by the server."),
                                                        NULL);
                                        */
                                        send(fd,"INVALID JSON Received",22,0);
                                        ev_io_stop(EV_A_ w);
                                        close(fd);
                                        return;
                                        //return close_connection(loop, w);
                                }
                        }
                }

        }
}


connect_watcher *
new_connector(EV_P_ char *addrstr,
              char *portstr)
{
        int errnum;
	int flags;

        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_NUMERICHOST;
        hints.ai_family = AF_INET;
	connect_watcher *c = NULL;

        int err = getaddrinfo(addrstr,
                                  portstr,
                                  &hints,
                                  &res);

        struct addrinfo *addr,*addr_base;
        addr = res;
        addr_base =  res;

        if (err) {
                DEBUG_ERR_LOG( "getaddrinfo failed: %s", gai_strerror(err));
                return NULL;
        }

        int fd = socket(addr->ai_addr->sa_family, SOCK_STREAM, 0);
	DEBUG_INFO("socket create status:%d",fd);
        if (fd == -1)
                return NULL;
/*
        if (set_nonblocking(fd) == -1)
                goto err_cleanup;
*/
        /*
          * Treat both an immediate connection and EINPROGRESS as success,
          * let the caller sort it out.
          */
        int status = connect(fd, addr->ai_addr, addr->ai_addrlen);
	perror("connect");
	DEBUG_INFO("connect status:%d",status);
        if ((status == 0) || ((status == -1) && (errno == EINPROGRESS))) {
                c = malloc(sizeof(connect_watcher));
                if (c)
                {
			bzero(c,sizeof(connect_watcher));
                        ev_io_init(&c->eio, connect_cb, fd, EV_WRITE);
                        c->addr = addr;
                        c->addr_base = addr_base;
                        return c;
                }
                /* else error */
        }
err_cleanup:

        //ev_io_stop(loop, &c->eio);
	ev_break(loop,EVBREAK_ONE);
	ev_unref(loop);
	ev_loop_destroy(loop);
        close(fd);
	DEBUG_ERR("~~~~~~~~~~~~~~~~~~~~~close %d",fd);
        return 0;
}

/**connect out end**/

void get_time_now(char * datetime) {
	time_t now;
        struct tm *tm_now;
       // char    datetime[200];
			     
        time(&now);
        tm_now = localtime(&now);
        strftime(datetime, 200, "%Y-%m-%d %H:%M:%S", tm_now);
					 	
	DEBUG_INFO("now datetime: %s", datetime);
					     
}

app_client_t * get_appt_by_ctx(jrpc_context *ctx)
{
	int fd = ((struct jrpc_connection *)(ctx->data))->fd;
	app_client_t * appt = g_app_client_list;
	while(appt != NULL)
	{
		if(appt->socket_fd == fd)
			return appt;
		appt = appt->next;
	}

	return NULL;
}

int access_check(jrpc_context * ctx)
{
/*
	if(g_ipconf_get_flag == 0)
		send_msg_to_server("127.0.0.1",5678,"{\"method\":\"ipconf_updated\"}");
*/
	int fd = ((struct jrpc_connection *)(ctx->data))->fd;
	app_client_t * appt = g_app_client_list;
	while(appt != NULL)
	{
		if(appt->socket_fd == fd)
			break;
		appt = appt->next;
	}
	
	if(appt && appt->current_state >= JOINED)
		return 1;
	else
		return 0;
}  

#ifdef SQL
static int appinfo_sql_callback(void *flag, int argc, char **argv, char **szColName)  
{  
	int i = 0;  
	          
	//printf("notused:0x%x, argc:%d\n", flag, argc);  
	for (i = 0; i < argc; i++)  
        {  
	        DEBUG_INFO("%s = %s\n", szColName[i], argv[i] == 0 ? "NULL" : argv[i]);  
		if(flag)
			strcpy(flag,"find");
        }  
	DEBUG_INFO("\n");  
			      
	return 0;  
}  

int create_sqlite_table()
{
	const char *sSQL1 = "create table appinfo(appid varchar(256) PRIMARY KEY, name varchar(64), type varchar(16), joindate datetime);";  
	const char *sSQL2 = "create table updateinfo( devtype varchar(32) PRIMARY KEY,version varchar(32), updatetime datetime,filename varchar(1024));";  
	const char *sSQL3 = "create table callhistory( calltime datetime PRIMARY KEY,person varchar(32), event varchar(512));";  
	const char *sSQL4 = "create table msghistory( time datetime PRIMARY KEY,url varchar(256));";  
	const char *sSQL5 = "create table seczonehistory( time datetime PRIMARY KEY,type varchar(32),seczone varchar(64),event varchar(512));";  
//	const char *sSQL2 = "insert into users values('wang', 20, '1989-5-4');";  
//	const char *sSQL3 = "select * from users;";  
		          
	sqlite3 *db = 0;  
	char *pErrMsg = 0;  
	int ret = 0;
	//连接数据库  
	ret = sqlite3_open(INNER_DB, &db);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));  
	        sqlite3_close(db);  
	        return -1;  
	}  
        DEBUG_INFO("数据库连接成功");  
	      
	//执行建表SQL 
	ret = sqlite3_exec(db, sSQL1, 0, 0, &pErrMsg);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "SQL create error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！  
	//        sqlite3_close(db);  
	//        return -1;  
	} 

	ret = sqlite3_exec(db, sSQL2, 0, 0, &pErrMsg);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "SQL create error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！  
	//        sqlite3_close(db);  
	//        return -1;  
	} 

	ret = sqlite3_exec(db, sSQL3, 0, 0, &pErrMsg);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "SQL create error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！  
	}

	ret = sqlite3_exec(db, sSQL4, 0, 0, &pErrMsg);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "SQL create error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！  
	}

	ret = sqlite3_exec(db, sSQL5, 0, 0, &pErrMsg);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "SQL create error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！  
	        sqlite3_close(db);  
	        return -1;  
	} 

        DEBUG_INFO("数据库建表成功！！");  
	sqlite3_close(db);  
	return 0;
}

int insert_data_to_db(const char *sqlstr)
{
	sqlite3 *db = 0;  
	char *pErrMsg = 0;  
	int ret = 0;

	//连接数据库  
	ret = sqlite3_open(INNER_DB, &db);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));  
	        sqlite3_close(db);  
	        return -1;  
	}  
        DEBUG_INFO("数据库连接成功");  
	      
	//执行查询操作 
	ret = sqlite3_exec(db, sqlstr, 0, 0, &pErrMsg);  
        if (ret != SQLITE_OK)  
        {  
		fprintf(stderr, "SQL insert error: %s\n", pErrMsg);  
        	sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！  
        	sqlite3_close(db);  
        	return -1;  
    	}  
     	DEBUG_INFO("数据库插入数据成功！"); 
	sqlite3_close(db);  
	return 0;

}
 
int query_appinfo_fromdb(char * sqlstr)
{
	sqlite3 *db = 0;  
	char *pErrMsg = 0;  
	int ret = 0;
	char flag[8];
	memset(flag,0,8);
	//const char *sSQL3 = "select * from appinfo where name='dd';";  

	//连接数据库  
	ret = sqlite3_open(INNER_DB, &db);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));  
	        sqlite3_close(db);  
	        return -1;  
	}  
	//执行查询操作 
	ret = sqlite3_exec(db, sqlstr, appinfo_sql_callback, flag, &pErrMsg);  
    	if (ret != SQLITE_OK)  
        {  
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg);  
	        sqlite3_close(db);  
	        return -1;  
    	}  
        DEBUG_INFO("数据库查询成功！！");  
	//关闭数据库  
	sqlite3_close(db);  
	if(strcmp(flag,"find") == 0)
		return 1;
	else
		return 0;
}

static int updateinfo_sql_callback(void *app, int argc, char **argv, char **szColName)  
{  
	int i = 0;  
	char returnstring[1024];
	memset(returnstring,0,1024);
	char urlstring[512];
	memset(urlstring,0,512);
        cJSON *err;
	//printf("notused:0x%x, argc:%d\n", flag, argc);  
	if(argv[1] == 0 || argv[2] == 0 ||argv[3] == 0 ) {
		err = cJSON_CreateString("app_update_no_new_version");
		cJSON_AddItemToObject(app,"app_update_info",err);		
		return 0;
	}

	if(strlen(g_localaddress) == 0)
		get_local_address();
        cJSON *result_root = cJSON_CreateObject();
        cJSON *string = cJSON_CreateString(argv[1]);
        cJSON_AddItemToObject(result_root,"version",string);
	snprintf(urlstring,512,"http://%s/update/%s",g_localaddress,argv[3]);
	string = cJSON_CreateString(urlstring);
        cJSON_AddItemToObject(result_root,"version",string);
	string = cJSON_CreateString(argv[2]);
        cJSON_AddItemToObject(result_root,"updatetime",string);

	cJSON_AddItemToObject(app,"app_update_info",result_root);
/*			
	snprintf(returnstring,1024,"{\"app_update_info\":{\"version\":\"%s\",\"url\":\"http://%s/update/%s\",\"updatetime\":\"%s\"}}",argv[1],g_localaddress,argv
[3],argv[2]);
	printf("%s",returnstring);		     
	sprintf(flag,"%s",returnstring);
*/ 
	return 0;  
}
  
cJSON * query_updateinfo_fromdb(char * sqlstr)
{
	sqlite3 *db = 0;  
	char *pErrMsg = 0;  
	int ret = 0;
	//const char *sSQL3 = "select * from appinfo where name='dd';";  

	//snprintf(insertsql,1024,"insert into updateinfo values ('%s','%s', '%s', '%s')",devtype, version, updatetime, filename);
	//连接数据库  
	ret = sqlite3_open(INNER_DB, &db);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));  
	        sqlite3_close(db);  
	        return NULL;  
	}  
	//执行查询操作 
        cJSON *app = cJSON_CreateObject();
	ret = sqlite3_exec(db, sqlstr, updateinfo_sql_callback, app, &pErrMsg);  
    	if (ret != SQLITE_OK)  
        {  
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg);  
	        sqlite3_close(db);  
		cJSON_Delete(app);
	        return NULL;  
    	}  
        DEBUG_INFO("数据库查询成功！！");  
	//关闭数据库  
	sqlite3_close(db);  
	return app;
}

static int callhistory_sql_callback(void *content_array, int argc, char **argv, char **szColName)  
{  
	int i = 0;  
	char urlstring[512];
	memset(urlstring,0,512);
	char person[512];
	memset(person,0,512);

	if(argv[0] == 0 || argv[1] == 0 ||argv[2] == 0 ) {
		return 0;
	}
	if(strlen(g_localaddress) == 0)
		get_local_address();
	cJSON *content_obj = cJSON_CreateObject();
	cJSON *string = cJSON_CreateString(argv[0]);
	cJSON_AddItemToObject(content_obj,"calltime",string);
	snprintf(person,512,"http://%s/callhistory/%s",g_localaddress,argv[1]);
	string = cJSON_CreateString(person);
	cJSON_AddItemToObject(content_obj,"person",string);
	string = cJSON_CreateString(argv[2]);
	cJSON_AddItemToObject(content_obj,"event",string);
	cJSON_AddItemToArray(content_array,content_obj);

	return 0;  
}

cJSON * query_callhistory_fromdb(char *sqlstr,int page,int total)
{
	sqlite3 *db = 0;  
	char *pErrMsg = 0;  
	int ret = 0;
	//const char *sSQL3 = "select * from appinfo where name='dd';";  

	//snprintf(insertsql,1024,"insert into updateinfo values ('%s','%s', '%s', '%s')",devtype, version, updatetime, filename);
	//连接数据库  
	ret = sqlite3_open(INNER_DB, &db);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));  
	        sqlite3_close(db);  
	        return NULL;  
	}  
	//执行查询操作 
        cJSON * content_array= cJSON_CreateArray();
	ret = sqlite3_exec(db, sqlstr, callhistory_sql_callback, content_array, &pErrMsg);  
    	if (ret != SQLITE_OK)  
        {  
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg);  
	        sqlite3_close(db);  
		cJSON_Delete(content_array);
	        return NULL;  
    	}  

	cJSON * callhistory = cJSON_CreateObject();
	cJSON_AddNumberToObject(callhistory,"page",page);
	cJSON_AddNumberToObject(callhistory,"total",total);
	cJSON_AddItemToObject(callhistory,"callhistory",content_array);

        DEBUG_INFO("数据库查询成功！！");  
	//关闭数据库  
	sqlite3_close(db);  
	return callhistory;
}

static int callhistory_count_callback(void *count, int argc, char **argv, char **szColName)  
{  

	strcpy(count,argv[0]);

	return 0;  
}

int query_callhistory_count(char *sqlstr)
{

	sqlite3 *db = 0;  
	char *pErrMsg = 0;  
	int ret = 0;
	char count[32] ;

	memset(count,0,32);
	//连接数据库  
	ret = sqlite3_open(INNER_DB, &db);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));  
	        sqlite3_close(db);  
	        return 0;  
	}  
	//执行查询操作 
	ret = sqlite3_exec(db, sqlstr, callhistory_count_callback, count, &pErrMsg);  
    	if (ret != SQLITE_OK)  
        {  
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg);  
	        sqlite3_close(db);  
	        return 0;  
    	}  

        DEBUG_INFO("数据库查询成功！！");  
	//关闭数据库  
	sqlite3_close(db);
  
	return atoi(count);
}

static int msghistory_sql_callback(void *content_array, int argc, char **argv, char **szColName)  
{  
	int i = 0;  
	char urlstring[512];
	memset(urlstring,0,512);
	char url[512];
	memset(url,0,512);
	
	if(argv[0] == 0 || argv[1] == 0) {
		return 0;
	}

	cJSON *content_obj = cJSON_CreateObject();
	cJSON *string = cJSON_CreateString(argv[0]);
	cJSON_AddItemToObject(content_obj,"time",string);
	//snprintf(url,512,"rtsp://%s:554/msghistory/%s",g_localaddress,argv[1]);
	snprintf(url,512,"%s%s",MSGHISTORY_PATH,argv[1]);
	//snprintf(url,512,"http://10.11.11.100/1251772650.mp4");

	string = cJSON_CreateString(url);
	cJSON_AddItemToObject(content_obj,"url",string);
	cJSON_AddItemToArray(content_array,content_obj);

	return 0;  
}

static int msghistory_count_callback(void *count, int argc, char **argv, char **szColName)  
{  

	strcpy(count,argv[0]);

	return 0;  
}

int query_msghistory_count(char *sqlstr)
{

	sqlite3 *db = 0;  
	char *pErrMsg = 0;  
	int ret = 0;
	char count[32] ;

	memset(count,0,32);
	//连接数据库  
	ret = sqlite3_open(INNER_DB, &db);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));  
	        sqlite3_close(db);  
	        return 0;  
	}  
	//执行查询操作 
	ret = sqlite3_exec(db, sqlstr, msghistory_count_callback, count, &pErrMsg);  
    	if (ret != SQLITE_OK)  
        {  
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg);  
	        sqlite3_close(db);  
	        return 0;  
    	}  

        DEBUG_INFO("数据库查询成功！！");  
	//关闭数据库  
	sqlite3_close(db);
  
	return atoi(count);
}

cJSON * query_msghistory_fromdb(char * sqlstr,int page,int total)
{
	sqlite3 *db = 0;  
	char *pErrMsg = 0;  
	int ret = 0;

	//连接数据库  
	ret = sqlite3_open(INNER_DB, &db);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));  
	        sqlite3_close(db);  
	        return NULL;  
	}  
	//执行查询操作 
        cJSON * content_array= cJSON_CreateArray();
	ret = sqlite3_exec(db, sqlstr, msghistory_sql_callback, content_array, &pErrMsg);  
    	if (ret != SQLITE_OK)  
        {  
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg);  
	        sqlite3_close(db);  
		cJSON_Delete(content_array);
	        return NULL;  
    	}  

	cJSON * msghistory = cJSON_CreateObject();
	cJSON_AddNumberToObject(msghistory,"page",page);
	cJSON_AddNumberToObject(msghistory,"total",total);
	cJSON_AddItemToObject(msghistory,"messagehistory",content_array);

        DEBUG_INFO("数据库查询成功！！");  
	//关闭数据库  
	sqlite3_close(db);  
	return msghistory;
}

static int seczonehistory_count_callback(void *count, int argc, char **argv, char **szColName)  
{  

	strcpy(count,argv[0]);

	return 0;  
}

int query_seczonehistory_count(char *sqlstr)
{

	sqlite3 *db = 0;  
	char *pErrMsg = 0;  
	int ret = 0;
	char count[32] ;

	memset(count,0,32);
	//连接数据库  
	ret = sqlite3_open(INNER_DB, &db);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));  
	        sqlite3_close(db);  
	        return 0;  
	}  
	//执行查询操作 
	ret = sqlite3_exec(db, sqlstr, seczonehistory_count_callback, count, &pErrMsg);  
    	if (ret != SQLITE_OK)  
        {  
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg);  
	        sqlite3_close(db);  
	        return 0;  
    	}  

        DEBUG_INFO("数据库查询成功！！");  
	//关闭数据库  
	sqlite3_close(db);
  
	return atoi(count);
}

//const char *sSQL5 = "create table seczonehistory( time datetime PRIMARY KEY,type varchar(32),seczone varchar(64),event varchar(512));";  
static int seczonehistory_sql_callback(void *content_array, int argc, char **argv, char **szColName)  
{  
	int i = 0;  
	
	if(argv[0] == 0 || argv[1] == 0 ||argv[2] == 0 ) {
		return 0;
	}

	cJSON *content_obj = cJSON_CreateObject();
	cJSON *string = cJSON_CreateString(argv[0]);
	cJSON_AddItemToObject(content_obj,"time",string);
	string = cJSON_CreateString(argv[1]);
	cJSON_AddItemToObject(content_obj,"type",string);
	string = cJSON_CreateString(argv[2]);
	cJSON_AddItemToObject(content_obj,"seczone",string);
	string = cJSON_CreateString(argv[3]);
	cJSON_AddItemToObject(content_obj,"event",string);
	cJSON_AddItemToArray(content_array,content_obj);

	return 0;  
}

cJSON * query_seczonehistory_fromdb(char * sqlstr,int page,int total)
{
	sqlite3 *db = 0;  
	char *pErrMsg = 0;  
	int ret = 0;

	//连接数据库  
	ret = sqlite3_open(INNER_DB, &db);  
	if (ret != SQLITE_OK)  
	{  
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));  
	        sqlite3_close(db);  
	        return NULL;  
	}  
	//执行查询操作 
        cJSON * content_array= cJSON_CreateArray();
	ret = sqlite3_exec(db, sqlstr, seczonehistory_sql_callback, content_array, &pErrMsg);  
    	if (ret != SQLITE_OK)  
        {  
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);  
	        sqlite3_free(pErrMsg);  
	        sqlite3_close(db);  
		cJSON_Delete(content_array);
	        return NULL;  
    	}  

	cJSON * seczonehistory = cJSON_CreateObject();
	cJSON_AddNumberToObject(seczonehistory,"page",page);
	cJSON_AddNumberToObject(seczonehistory,"total",total);
	cJSON_AddItemToObject(seczonehistory,"seczone_history",content_array);

        DEBUG_INFO("数据库查询成功！！");  
	//关闭数据库  
	sqlite3_close(db);  
	return seczonehistory;
}
#endif

void create_basic_conf_file(void)
{
	FILE    *   ini ;
	int i = 0;

    	ini = fopen(BASIC_CONF, "w");
    	fprintf(ini,
    	"#\n"
    	"# basic configure file\n"
    	"#\n"
    	"\n"
	"#小区门口室外机\n"
	"#\n"	
    	"[gateouterbox]\n"
    	"\n"
    	"ip = %s ;\n"
    	"\n"
    	"\n"
	"#单元门口室外机\n"
	"#\n"	
    	"[outerbox]\n"
    	"\n"
    	"ip = %s ;\n"
    	"\n"
    	"\n"
	"#负一楼门口室外机\n"
	"#\n"	
    	"[outerbox_1]\n"
    	"\n"
    	"ip = %s ;\n"
    	"\n"
    	"\n"
	"#物业中心服务器\n"
	"#\n"	
    	"[propertyserver]\n"
    	"\n"
    	"ip = %s ;\n"
    	"\n"
    	"\n"
    	"[streamserver]\n"
    	"\n"
    	"ip = %s ;\n"
    	"\n"
    	"\n"
    	"[family]\n"
    	"\n"
    	"ip = %s ;\n"
    	"\n"
    	"\n"
	"#物业呼叫中心\n"
	"#\n"	
	"[callcenter]\n"
	"\n"
	"ip = %s ;\n"
	"\n\n"
	"#物业报警处理中心\n"
	"#\n"	
	"[alarmcenter]\n"
	"\n"
	"ip = %s ;\n"
	"\n\n"
	"#物业岗亭机\n"
	"#\n"	
	"[sentrybox]\n"
	"\n"
	"count = %d ;\n"
	"\n"
	"\n",
    	g_baseconf.gateouterboxip,
    	g_baseconf.outerboxip,
    	g_baseconf.outerboxip_1,
    	g_baseconf.propertyip,
    	g_baseconf.streamserverip,
    	g_baseconf.familyip,
    	g_baseconf.callcenterip,
    	g_baseconf.alarmcenterip,
	g_baseconf.sentryboxcount
    	);
	
	for(i=0;i<g_baseconf.sentryboxcount;i++) {
		fprintf(ini,
		"[sentrybox%d]\n"
		"name= %s ;\n"
		"ipaddr= %s ;\n\n",
		i,g_baseconf.sentrybox[i].name,g_baseconf.sentrybox[i].ipaddr);
	}	
    	fclose(ini);
/*
    ini = fopen(VERSION_CONF, "w");
    fprintf(ini,
    "#\n"
    "# innerbox version\n"
    "#\n"
    "\n"
    "[version]\n"
    "\n"
    "ver = %s ;\n"
    "\n",
    g_baseconf.version
    );
    fclose(ini);
*/
}

int get_version_fromini( char * ini_name)
{
	
    	dictionary  *   ini ;
    	char        *   s ;
    	ini = iniparser_load(ini_name);
    	if (ini==NULL) {
        	fprintf(stderr, "cannot parse file: %s\n", ini_name);
        	return -1 ;
    	}

    	DEBUG_INFO("version:");
    	s = iniparser_getstring(ini, "version:ver", NULL);
    	DEBUG_INFO("VER: [%s]\n", s ? s : "UNDEF");
    	if(s)
    		strcpy(g_baseconf.version,s); 

    	iniparser_freedict(ini);
    	return 0 ;
}

int get_baseconf_fromini(char * ini_name)
{
    	dictionary  *   ini ;

    	/* Some temporary variables to hold query results */
    	int             b ;
    	int             i ;
    	double          d ;
    	char        *   s ;
	char tmpstr[128];
	int j;

    	ini = iniparser_load(ini_name);
    	if (ini==NULL) {
        	fprintf(stderr, "cannot parse file: %s\n", ini_name);
        	return -1 ;
    	}
    	//iniparser_dump(ini, stderr);

    	/* Get attributes */

    	DEBUG_INFO("outerboxip:");
    	s = iniparser_getstring(ini, "outerbox:ip", NULL);
    	DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    	if(s)
    		strcpy(g_baseconf.outerboxip,s); 
    	
	DEBUG_INFO("gateouterboxip:");
    	s = iniparser_getstring(ini, "gateouterbox:ip", NULL);
    	DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    	if(s)
    		strcpy(g_baseconf.gateouterboxip,s); 
	
	DEBUG_INFO("outerboxip_1:");
    	s = iniparser_getstring(ini, "outerbox_1:ip", NULL);
    	DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    	if(s)
    		strcpy(g_baseconf.outerboxip_1,s); 
    // i = iniparser_getint(ini, ":year", -1);
    // printf("Year:      [%d]\n", i);
/*
    	printf("outerinterface:\n");
    	s = iniparser_getstring(ini, "outerinterface:ip", NULL);
    	printf("IP: [%s]\n", s ? s : "UNDEF");
    	if(s)
    		strcpy(g_baseconf.outerinterfaceip,s); 
*/   
    	DEBUG_INFO("property:");
    	s = iniparser_getstring(ini, "propertyserver:ip", NULL);
    	DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    	if(s)
    		strcpy(g_baseconf.propertyip,s); 
    
    	DEBUG_INFO("stream:");
    	s = iniparser_getstring(ini, "streamserver:ip", NULL);
    	DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    	if(s)
    		strcpy(g_baseconf.streamserverip,s); 

    	DEBUG_INFO("family:");
    	s = iniparser_getstring(ini, "family:ip", NULL);
    	DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    	if(s)
    		strcpy(g_baseconf.familyip,s); 
    	
	DEBUG_INFO("callcenter:");
    	s = iniparser_getstring(ini, "callcenter:ip", NULL);
    	DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    	if(s)
    		strcpy(g_baseconf.callcenterip,s); 
	
	DEBUG_INFO("alarmcenter:");
    	s = iniparser_getstring(ini, "alarmcenter:ip", NULL);
    	DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    	if(s)
    		strcpy(g_baseconf.alarmcenterip,s); 
	//sentry box info
	DEBUG_INFO("sentrybox:");
	i = iniparser_getint(ini, "sentrybox:count",0);	
	DEBUG_INFO("count:[%d]",i);
	g_baseconf.sentryboxcount = i;

	for(j=0;j<i;j++) {
		bzero(tmpstr,128);
		sprintf(tmpstr,"sentrybox%d:name",j);
		s = iniparser_getstring(ini,tmpstr,NULL);
		DEBUG_INFO("sentrybox%d:%s",j,s?s:"UNDEF");
		if(s)
			strncpy(g_baseconf.sentrybox[j].name,s,64);
		bzero(tmpstr,128);
		sprintf(tmpstr,"sentrybox%d:ipaddr",j);
		s = iniparser_getstring(ini,tmpstr,NULL);
		DEBUG_INFO("sentrybox%d:%s",j,s?s:"UNDEF");
		if(s)
			strncpy(g_baseconf.sentrybox[j].ipaddr,s,64);
		
	}	


    //d = iniparser_getdouble(ini, "wine:alcohol", -1.0);
    //printf("Alcohol:   [%g]\n", d);

    	iniparser_freedict(ini);

	if((strlen(g_baseconf.callcenterip) == 0) && (strlen(g_baseconf.propertyip)>0))
		strcpy(g_baseconf.callcenterip,g_baseconf.propertyip);
	if((strlen(g_baseconf.alarmcenterip) == 0) && (strlen(g_baseconf.propertyip)>0))
		strcpy(g_baseconf.alarmcenterip,g_baseconf.propertyip);
    	return 0 ;
}

//conig the address of the outer interface  
void do_set_ip()
{
	//copy the ip to /etc/interface or  /etc/network
	//
}

//engineering setup when get request from app
cJSON * init_setup(jrpc_context * ctx, cJSON * params, cJSON *id) {
	cJSON *param = NULL;

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

	if(params == NULL)
		return cJSON_CreateString("init setup null params!");
	//process params
	param = cJSON_GetObjectItem(params, "outerboxip");
	if(param != NULL)
		if(param->type == cJSON_String)
			strcpy(g_baseconf.outerboxip,param->valuestring);

	param = cJSON_GetObjectItem(params, "outerinterfaceip");
	if(param != NULL)
		if(param->type == cJSON_String)
			strcpy(g_baseconf.outerinterfaceip,param->valuestring);

	param = cJSON_GetObjectItem(params, "familyip");
	if(param != NULL)
		if(param->type == cJSON_String)
			strcpy(g_baseconf.familyip,param->valuestring);

	param = cJSON_GetObjectItem(params, "propertyip");
	if(param != NULL)
		if(param->type == cJSON_String)
			strcpy(g_baseconf.propertyip,param->valuestring);

	DEBUG_INFO("init ip:%s %s %s",g_baseconf.outerboxip,g_baseconf.outerinterfaceip,g_baseconf.propertyip);
	//do the setup 
	do_set_ip();

	create_basic_conf_file();

	return cJSON_CreateString("init_setup_has_done");
		
}

cJSON * msgpush_to_app()
{
        char urlstring[1024];
	if(strlen(g_localaddress) == 0)
		get_local_address();
        cJSON *result_root = cJSON_CreateObject();
        cJSON *string = cJSON_CreateString("msgpush");
        cJSON_AddItemToObject(result_root,"method",string);
        cJSON *content_obj = cJSON_CreateObject();
        string = cJSON_CreateString("天气预报");
        cJSON_AddItemToObject(content_obj,"title",string);
        string = cJSON_CreateNumber(1);
        cJSON_AddItemToObject(content_obj,"level",string);
        sprintf(urlstring,"http://%s/index.html",g_localaddress);
        //string = cJSON_CreateString("http://192.168.1.137/index.html");
        string = cJSON_CreateString(urlstring);
        cJSON_AddItemToObject(content_obj,"url",string);
        cJSON_AddItemToObject(result_root,"content",content_obj);

//      char *str_result = cJSON_Print(result_root);
//      cJSON_Delete(result_root);
//      cJSON_Delete(param_obj);
        return result_root;
}

//push message to app
cJSON * msg_push(jrpc_context * ctx, cJSON * params, cJSON *id) {
	int i = 0;
	app_client_t * appt = g_app_client_list;
	int ret = 0;
	char * msgstr;
	char *tmpstr;	
//#ifdef ANDROID
	//just resend the packet from property
        cJSON *result_root = cJSON_CreateObject();
        cJSON *string = cJSON_CreateString("msgpush");
        cJSON_AddItemToObject(result_root,"method",string);
	cJSON_AddItemToObject(result_root,"params",params);
	msgstr = cJSON_Print(result_root);
	tmpstr = malloc(strlen(msgstr)+2);
	memset(tmpstr,0,strlen(msgstr)+2);
	strcpy(tmpstr,msgstr);
	strncat(tmpstr,"^",1);
	cJSON_Delete(result_root);
//#else 
	//need to change the url or the www server ip addr
	//because the app can not connect to the property directly
/*	cJSON * msgobj = msgpush_to_app();
	msgstr = cJSON_Print(msgobj);
	tmpstr = malloc(strlen(msgstr)+2);
	memset(tmpstr,0,strlen(msgstr)+2);
	strcpy(tmpstr,msgstr);
	strncat(tmpstr,"^",1);

	cJSON_Delete(msgobj);*/	
//#endif
	while(appt) {
		char raddress[128];
		bzero(raddress,128);
		int rport = get_peer_address(appt->socket_fd,raddress);
		if(rport != appt->port || strcmp(appt->ip_addr_str,raddress) != 0) {
			appt = appt->next;
			continue;
		}

		if(appt->current_state >= JOINED ) {
			int lport = get_local_sockinfo(appt->socket_fd);
			if(lport>0 && lport == IN_PORT) {
				ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
			} else
				appt->current_state = OFFLINE;
			//ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
			DEBUG_INFO("msg write:%s %d",appt->ip_addr_str,ret);
		}

		if(ret == -1)
			appt->current_state = OFFLINE;

		appt = appt->next;	
	}

	free(msgstr);//?????need it
	free(tmpstr);

	return cJSON_CreateString("msg_has_push_to_app");
}

cJSON * updatemsg_to_app(char *devtype, char *urlstring, char *version, char *updatetime)
{

        cJSON *updatemsg = cJSON_CreateObject();
        cJSON *string = cJSON_CreateString("update_push");
        cJSON_AddItemToObject(updatemsg,"method",string);
        cJSON *param_obj = cJSON_CreateObject();
        string = cJSON_CreateString(devtype);
        cJSON_AddItemToObject(param_obj,"devtype",string);
        string = cJSON_CreateString(urlstring);
        cJSON_AddItemToObject(param_obj,"url",string);
        string = cJSON_CreateString(version);
        cJSON_AddItemToObject(param_obj,"version",string);
        string = cJSON_CreateString(updatetime);
        cJSON_AddItemToObject(param_obj,"updatetime",string);
        cJSON_AddItemToObject(updatemsg,"params",param_obj);

        return updatemsg;
}
/*
int rc4_crypt_file(char *infile,char *outfile) 
{

	char *addr;
	int fd;
	struct stat sb;
	size_t length;
	rc4_state state;
	char *outbuf;
	FILE *wfd;

	fd = open(infile, O_RDONLY);
	if( fd == -1)
		return -1;

	if(fstat(fd, &sb) == -1)
		return -1;
	length = sb.st_size;
	addr = mmap(NULL, length , PROT_READ,
                      MAP_PRIVATE, fd, 0);
	outbuf = malloc(length);
	memset(outbuf,0,length);

	rc4_init(&state,"llysc,ysykr.yhsbj,cfcys.",24);
	rc4_crypt(&state,addr,outbuf,length);
	wfd  = fopen(outfile,"w");

	fwrite(outbuf,1,length,wfd);
	fclose(wfd);
	munmap(addr,length);
	free(outbuf);
	return 0;
}
*/

char g_urlstring[1024];
void * do_softupdate()
{
	pthread_detach(pthread_self());
	char cmdstr[512];
	bzero(cmdstr,512);
	//wget the update package
#ifdef ANDROID	
	chdir("/data/local/tmp/");
	snprintf(cmdstr,512,"wget -O inner_update_package.tgz %s",g_urlstring);
	system(cmdstr);
	rc4_crypt_file("inner_update_package.tgz","inner_update_package_dec.tgz");
	system("tar zxvf inner_update_package_dec.tgz");
	system("rm -rf inner_update_package*.tgz");
	system("/data/local/tmp/inner_update.sh");
#else
	chdir("/");
	snprintf(cmdstr,512,"wget -O inner_update_package.tgz %s",g_urlstring);
	system(cmdstr);
	rc4_crypt_file("inner_update_package.tgz","inner_update_package_dec.tgz");
	system("tar zxvf inner_update_package_dec.tgz");
	system("rm -rf inner_update_package*.tgz");
	system("/tmp/inner_update.sh");
#endif

}

cJSON * softupdate_innerbox(jrpc_context * ctx, cJSON * params, cJSON *id) 
{

	cJSON *param = NULL;
	char version[32];
	bzero(version,32);
#ifdef DEBUG
	char * str = cJSON_Print(params);
	DEBUG_INFO("update:%s",str);
	free(str);
#endif
	bzero(g_urlstring,1024);
	param = cJSON_GetObjectItem(params, "version");
	if(param != NULL)
		if(param->type == cJSON_String)
			strncpy(version,param->valuestring,32);
		else
			return cJSON_CreateString(WRONG_PARAMETER);
	else
		return cJSON_CreateString(WRONG_PARAMETER);

	if(strcmp(g_baseconf.version,version) == 0 )
	{
		return cJSON_CreateString("Version_is_same_as_current");
	}

	param = cJSON_GetObjectItem(params, "url");
	if(param != NULL)
		if(param->type == cJSON_String)
			strncpy(g_urlstring,param->valuestring,1024);
		else
			return cJSON_CreateString(WRONG_PARAMETER);
	else
		return cJSON_CreateString(WRONG_PARAMETER);

	g_update_state = 1;
	
	create_worker(do_softupdate,NULL);
	return cJSON_CreateString("doing_softupdate_for_innerbox_just_waiting");	
}
//get the update push message of 
//{"method":"update_push","params":{"type":"innerbox/outerbox/android-pad/android-phone/ios-pad/ios-phone","url":"http://x.x.x.x/package","filename":"fff.tgz","version":"1.1","updatetime":"2017-10-1 12:00:00"}}
cJSON * update_push(jrpc_context * ctx, cJSON * params, cJSON *id) {

	cJSON *param = NULL;
	cJSON *msgobj = NULL;
	app_client_t * appt = g_app_client_list;
	char devtype[16];
	char urlstring[1024];
	char filename[512];
	char version[32];
	char updatetime[32];
	char *msgstr;
	char *tmpstr;
	int ret = 0;
	char insertsql[1024];
	char cmdstr[512];

	memset(devtype,0,16);
	memset(urlstring,0,1024);
	memset(version,0,32);
	memset(updatetime,0,32);
	memset(insertsql,0,1024);
	memset(filename,0,512);
	memset(cmdstr,0,512);

	if(params == NULL)
		return cJSON_CreateString("update push null params!");
	//process params
	param = cJSON_GetObjectItem(params, "type");
	if(param != NULL)
		if(param->type == cJSON_String)
			strncpy(devtype,param->valuestring,16);
		else
			return cJSON_CreateString(WRONG_PARAMETER);
	else
		return cJSON_CreateString(WRONG_PARAMETER);

	param = cJSON_GetObjectItem(params, "url");
	if(param != NULL)
		if(param->type == cJSON_String)
			strncpy(urlstring,param->valuestring,1024);
		else
			return cJSON_CreateString(WRONG_PARAMETER);
	else
		return cJSON_CreateString(WRONG_PARAMETER);

	param = cJSON_GetObjectItem(params, "filename");
	if(param != NULL)
		if(param->type == cJSON_String)
			strncpy(filename,param->valuestring,512);
		else
			return cJSON_CreateString(WRONG_PARAMETER);
	else
		return cJSON_CreateString(WRONG_PARAMETER);

	param = cJSON_GetObjectItem(params, "version");
	if(param != NULL)
		if(param->type == cJSON_String)
			strncpy(version,param->valuestring,32);
		else
			return cJSON_CreateString(WRONG_PARAMETER);
	else
		return cJSON_CreateString(WRONG_PARAMETER);

	param = cJSON_GetObjectItem(params, "updatetime");
	if(param != NULL)
		if(param->type == cJSON_String)
			strncpy(updatetime,param->valuestring,32);
		else
			return cJSON_CreateString(WRONG_PARAMETER);
	else
		return cJSON_CreateString(WRONG_PARAMETER);
	
	//insert to database
	
	snprintf(insertsql,1024,"insert into updateinfo values ('%s','%s', '%s', '%s')",devtype, version, updatetime, filename);
	DEBUG_INFO("%s",insertsql);
#ifdef SQL
	ret = insert_data_to_db(insertsql);
	if(ret != 0) {
		snprintf(insertsql,1024,"update updateinfo set filename = '%s', version='%s',updatetime = '%s' where devtype = '%s'",filename, version, updatetime,  devtype);
		insert_data_to_db(insertsql);
	}
#endif
	msgobj = updatemsg_to_app(devtype,urlstring,version,updatetime);
	msgstr = cJSON_Print(msgobj);
	tmpstr = malloc(strlen(msgstr)+2);
	strcpy(tmpstr,msgstr);
	strncat(tmpstr,"^",1);

	while(appt != NULL) {
		if(strcmp(appt->type,devtype) == 0) {
			char raddress[128];
			bzero(raddress,128);
			int rport = get_peer_address(appt->socket_fd,raddress);
			if(rport != appt->port || strcmp(appt->ip_addr_str,raddress) != 0) {
				appt = appt->next;
				continue;
			}

			if(appt->current_state >= JOINED) {
				int lport = get_local_sockinfo(appt->socket_fd);
				if(lport>0 && lport == IN_PORT) {
					ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
				} else
					appt->current_state = OFFLINE;
				if(ret == -1)
					appt->current_state = OFFLINE;
			}	
		}
				
		appt = appt->next;
	}
	
	cJSON_Delete(msgobj);	
	free(msgstr);
	free(tmpstr);

	//type = innerbox
	if(strcmp(devtype,"innerbox") == 0) {
		chdir("/");
		snprintf(cmdstr,512,"wget %s",urlstring);
		system(cmdstr);
		rc4_crypt_file(filename,"test.tgz");
		system("ls -l");
	}
	return cJSON_CreateString("update_push_has_done");
}

cJSON * opendoorcmd_to_app(app_client_t *appt)
{
        //char urlstring[1024];

        cJSON *result_root = cJSON_CreateObject();
        cJSON *string = cJSON_CreateString("opendoorplease");
        cJSON_AddItemToObject(result_root,"method",string);
        cJSON *param_obj = cJSON_CreateObject();
        string = cJSON_CreateString("001");
        cJSON_AddItemToObject(param_obj,"doorid",string);
	if(strlen(g_localaddress) == 0)
		get_local_address();
        //sprintf(urlstring,"rtsp://%s:554/ch01_sub.h264",g_localaddress);
        //sprintf(urlstring,"rtsp://%s:554/user=admin_password=tlJwpbo6_channel=1_stream=1.sdp?real_stream",g_localaddress);
        //sprintf(urlstring,"rtsp://admin:admin@%s:554/stream1",g_localaddress);
#ifdef ANDROID
        snprintf(g_stream_outerbox,1024,"rtsp://admin:admin@%s:554/stream1",g_last_callin_address);
#else
	if(strcmp(g_last_callin_address,g_baseconf.gateouterboxip) == 0)
        	snprintf(g_stream_outerbox,1024,"rtsp://admin:admin@%s:555/stream1",g_localaddress);
	else
        	snprintf(g_stream_outerbox,1024,"rtsp://admin:admin@%s:554/stream1",g_localaddress);
#endif
        string = cJSON_CreateString(g_stream_outerbox);
        cJSON_AddItemToObject(param_obj,"video",string);
        //sprintf(urlstring,"rtmp://%s:1935/live",g_localaddress);
        string = cJSON_CreateString(appt->audiostream_url);
        cJSON_AddItemToObject(param_obj,"audio",string);
        cJSON_AddItemToObject(result_root,"params",param_obj);

//      char *str_result = cJSON_Print(result_root);
//      cJSON_Delete(result_root);
//      cJSON_Delete(param_obj);
        return result_root;
}

char * generate_opendoorcmd_to_app(app_client_t *appt)
{
	
	cJSON * opendoor = opendoorcmd_to_app(appt);
	char * openstr = cJSON_Print(opendoor);
	char *tmpstr = malloc(strlen(openstr)+2);
	time_t now;
	char timestr[64];
	char cmdstr[1024];
	memset(tmpstr,0,strlen(openstr)+2);
	strcpy(tmpstr,openstr);
	strncat(tmpstr,"^",1);

	free(openstr);
	cJSON_Delete(opendoor);
	
	return tmpstr;

}

void callhistory_log_to_db(char *time,char *person,char *event)
{
	char sqlstr[1024];
	memset(sqlstr,0,1024);
	
	snprintf(sqlstr,1024,"insert into callhistory values ('%s','%s', '%s')",time, person,event);
#ifdef SQL
	insert_data_to_db(sqlstr);
#endif
}

cJSON * open_door(jrpc_context * ctx, cJSON * params, cJSON *id) {
	int i = 0;
	int ret = 0;
	cJSON *param = NULL;
	char cmdstr[1024];
	time_t now;

	bzero(g_last_callin_address,64);
	if(params != NULL)
		param = cJSON_GetObjectItem(params, "ipaddr");
	else
		return cJSON_CreateString("open_door_wrong_parameter");

	if(param != NULL)
		if(param->type == cJSON_String)
			strncpy(g_last_callin_address,param->valuestring,64);
	else
		return cJSON_CreateString("open_door_wrong_parameter");
	//line is busy


//	char *tmpstr =  generate_opendoorcmd_to_app(appt)
/*
	cJSON * opendoor = opendoorcmd_to_app();
	char * openstr = cJSON_Print(opendoor);
	char *tmpstr = malloc(strlen(openstr)+2);
	time_t now;
	char timestr[64];
	char cmdstr[1024];
	memset(tmpstr,0,strlen(openstr)+2);
	strcpy(tmpstr,openstr);
	strncat(tmpstr,"^",1);
	
*/
	char timestr[64];
	memset(cmdstr,0,1024);
	memset(timestr,0,64);
	int busy = 0;
	char raddress[128];
	bzero(raddress,128);
	char peeraddress[128];
	bzero(peeraddress,128);

	int outerport = get_peer_address(((struct jrpc_connection*)(ctx->data))->fd,peeraddress);
	DEBUG_INFO("open_door from :%s",peeraddress);
	app_client_t * appt = g_app_client_list;
	while(appt) {
		int rport = get_peer_address(appt->socket_fd,raddress);
		if(rport != appt->port || strcmp(appt->ip_addr_str,raddress) != 0) {
			appt->current_state =  OFFLINE;
			appt = appt->next;
			continue;
		}

		if(appt->current_state >= JOINED && appt->current_state <CALLING) {
			int lport = get_local_sockinfo(appt->socket_fd);
			if(lport>0 && lport == IN_PORT)  {
				char *tmpstr =  generate_opendoorcmd_to_app(appt);
				ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
				free(tmpstr);
				appt->current_state = CONNECTING;
				if(ret > 0) {
					appt->line_state->calltype = OUTER2APP;		
					update_app_line_state(appt,OUTER2APP,outerport,peeraddress,((struct jrpc_connection*)(ctx->data))->fd);
					busy++;	
				}
				DEBUG_INFO("opendoor cmd to:%s %d",appt->ip_addr_str,ret);
			} else
				appt->current_state = OFFLINE;
		}
		if(ret == -1) {
			appt->current_state = OFFLINE;
		}

		appt = appt->next;	
	}

	if(busy==0)
		return cJSON_CreateString("line_is_busy");

	/*
	for(i=0;i<MAX_FD_NUMBER;i++) {
		if(connection_fd_array[i] > 0)
		{
			write(connection_fd_array[i],tmpstr,strlen(tmpstr));
			printf("fd:%d\n",connection_fd_array[i]);
		}
	}
	*/
	//cJSON_Delete(opendoor);	
	//free(openstr);
	//free(tmpstr);

	//do history record 
	//may write it to database
	//grab a picture of the caller
	memset(g_last_caller,0,64);
	snprintf(g_last_caller,64,"%d.jpg",(int)time(&now));
	snprintf(cmdstr,1024,"ffmpeg -y -i %s -vframes 1 %s%s &",g_stream_outerbox,CALLHISTORY_PATH,g_last_caller);
	//system(cmdstr);
	get_time_now(timestr);
	callhistory_log_to_db(timestr,g_last_caller,"请求开门");
	//close(((struct jrpc_connection*)(ctx->data))->fd);
	return cJSON_CreateString("opendoor_request_sent");
}

cJSON * say_hello(jrpc_context * ctx, cJSON * params, cJSON *id) {
	return cJSON_CreateString("Hello!");
}

app_client_t * malloc_init_app_client_t(void)
{
	app_client_t *appt = malloc(sizeof(app_client_t));
	bzero(appt,sizeof(app_client_t));

	appt->line_state = malloc(sizeof(call_line_state_t));
	bzero(appt->line_state,sizeof(call_line_state_t));
/*
	appt->socket_fd = 0;
	appt->port = 0;
	memset(appt->app_dev_id,0,512);
	memset(appt->ip_addr_str,0,64);
	appt->online = 0;
	appt->joined = 0;
	appt->next = 0;
	memset(appt->name,0,64);
	memset(appt->type,0,16);
	memset(appt->version,0,32);
*/
	return appt;
}
//update info of APP client
app_client_t * update_app_client_info(int fd, const char * peeraddr, int port, char *app_dev_id, int state, char *name, char *type) {
	app_client_t * appt = g_app_client_list;
	app_client_t * newappt = 0;
	app_client_t * lastappt = 0;
	int find = 0;
	char insertsql[1024];
	char datetime[200];
	int ret = 0;

	if(appt) {
		while(appt != NULL) {
			if(appt->socket_fd == fd) {
				//appt->socket_fd = fd;
				if(peeraddr != NULL)
					strcpy(appt->ip_addr_str,peeraddr);
				if(port > 0)
					appt->port = port;
				appt->current_state = state;
				if(name != NULL)
					strcpy(appt->name,name);
				if(type != NULL)
					strcpy(appt->type,type);
				if(app_dev_id != NULL)
					strcpy(appt->app_dev_id,app_dev_id);
				find++;
				newappt = appt;
			}
			lastappt = appt;
			appt = appt->next;
		}
	}
		
	if(find == 0) {
		newappt = malloc_init_app_client_t();
		newappt->socket_fd = fd;
		newappt->port = port;
		if(app_dev_id != NULL)
			strcpy(newappt->app_dev_id,app_dev_id);
		if(peeraddr != NULL)
			strcpy(newappt->ip_addr_str,peeraddr);
		newappt->current_state = state;
		if(name != NULL)
				strcpy(newappt->name,name);
		if(type != NULL)
				strcpy(newappt->type,type);
		if(!lastappt) {
			g_app_client_list = newappt;
		}
		else
			lastappt->next = newappt;
	}	

	//insert the appinfo to database
	if(newappt->current_state >= JOINED) {

		memset(insertsql,0,1024);
		memset(datetime,0,200);
		get_time_now(datetime);
		snprintf(insertsql,1024,"insert into appinfo values ('%s','%s', '%s', '%s')",newappt->app_dev_id, name,newappt->type, datetime);
		DEBUG_INFO("%s",insertsql);
#ifdef SQL
		ret = insert_data_to_db(insertsql);
		if(ret != 0) {
			snprintf(insertsql,1024,"update appinfo set name = '%s', type='%s',joindate = '%s' where appid = '%s'",newappt->name,newappt->type,datetime,  newappt->app_dev_id);
			insert_data_to_db(insertsql);
		}
#endif
	}
	
	return newappt;

}

void travel_app_list()
{
	app_client_t * appt = g_app_client_list;
	while(appt != NULL)
	{
		DEBUG_INFO("app id:%s ip:%s port:%d state:%d socket:%d ",appt->app_dev_id,appt->ip_addr_str,appt->port,appt->current_state,appt->socket_fd);
		appt = appt->next;
	}
}

void get_local_address1(int fd)
{
	struct sockaddr_in  localAddr;//local address
	int localLen = sizeof(localAddr);
	char ipAddr[INET_ADDRSTRLEN];
	memset(&localAddr, 0, sizeof(localAddr));
	//get the local addr
	if((g_localaddress[0]=='\0') || (strcmp(g_localaddress,"127.0.0.1") == 0))
	        /*if(((struct jrpc_server *) w->data)->port_number == IN_PORT) */{
			getsockname(fd, (struct sockaddr *)&localAddr, &localLen);
			DEBUG_INFO("local:%s",inet_ntop(AF_INET, &localAddr.sin_addr, ipAddr, sizeof(ipAddr)));
			strcpy(g_localaddress,inet_ntop(AF_INET, &localAddr.sin_addr, ipAddr, sizeof(ipAddr)));
		}

}

void get_local_address()
{

        struct ifaddrs * ifAddrStruct=NULL;    
        void * tmpAddrPtr=NULL;    
        getifaddrs(&ifAddrStruct);   
        while (ifAddrStruct!=NULL) {        
		if(ifAddrStruct->ifa_addr == NULL) {
			
			ifAddrStruct=ifAddrStruct->ifa_next;    
			continue;
		}

//		printf("ifAddrStruct:%d \n",ifAddrStruct);
	//	printf("ifAddrStruct->ifa_addr:%d \n",ifAddrStruct->ifa_addr);
	//	printf("ifAddrStruct->ifa_addr->sa_family:%d \n",ifAddrStruct->ifa_addr->sa_family);
                if (ifAddrStruct->ifa_addr->sa_family==AF_INET) {   
                        tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;            
                        char addressBuffer[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);            
                        DEBUG_INFO("%s IP Address %s", ifAddrStruct->ifa_name, addressBuffer);
			if(strncmp(ifAddrStruct->ifa_name,"eth0",4) == 0) {
#ifdef ANDROID
//				if(strncmp(addressBuffer,g_baseconf.propertyip,3) == 0)
#else
#endif
					strcpy(g_localaddress,addressBuffer);   

			}    
			if(strncmp(ifAddrStruct->ifa_name,"eth1",4) == 0)
				strcpy(g_baseconf.outerinterfaceip,addressBuffer);       
                } else if (ifAddrStruct->ifa_addr->sa_family==AF_INET6) {             
                        tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;            
                        char addressBuffer[INET6_ADDRSTRLEN];            
                        inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);            
                        DEBUG_INFO("%s IP Address %s", ifAddrStruct->ifa_name, addressBuffer);        
	/*		if(strncmp(ifAddrStruct->ifa_name,"eth0",4) == 0)
				strcpy(g_localaddress,addressBuffer);       
			if(strncmp(ifAddrStruct->ifa_name,"eth1",4) == 0)
				strcpy(g_baseconf.outerinterfaceip,addressBuffer);      */ 
                }        
		ifAddrStruct=ifAddrStruct->ifa_next;    
        }    
	if(ifAddrStruct)
		free(ifAddrStruct);
#ifdef ANDROID
	if(strlen(g_baseconf.outerinterfaceip) == 0)
		strcpy(g_baseconf.outerinterfaceip,g_localaddress);
#endif
}

int get_peer_address(int fd, char *peeraddress)
{
	//struct sockaddr_in  peerAddr;//连接的对端地址
	struct sockaddr_storage  peerAddr;//连接的对端地址
	socklen_t peerLen = sizeof(peerAddr);
	int ret = 0;
	char ipstr[128];
	bzero(ipstr,128);
	int port =0;
	char *ptr = NULL;
	memset(&peerAddr, 0, sizeof(peerAddr));
	//get the app client addr
	ret = getpeername(fd, (struct sockaddr *)&peerAddr, &peerLen);
	if(ret == -1) perror("getpeername error!");
	if (peerAddr.ss_family == AF_INET) { 
		struct sockaddr_in *s = (struct sockaddr_in *)&peerAddr; 
		port = ntohs(s->sin_port); 
		inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr); 
		strcpy(peeraddress,ipstr);
	} else if(peerAddr.ss_family == AF_INET6) { 
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&peerAddr; 
		port = ntohs(s->sin6_port); 
		inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr); 
		ptr = strtok(ipstr,":");	
		while(ptr!=NULL) {
			strcpy(peeraddress,ptr);
			ptr = strtok(NULL,":");
		}
	}

	//strcpy(peeraddress,ipstr);
	DEBUG_INFO("connected peer address = %s:%d",peeraddress,port );

	return port;
}

cJSON * packet42_create(char *qrcode, char *name, char *type)
{
	cJSON *result_root = cJSON_CreateObject();
	cJSON *params = cJSON_CreateObject();
	cJSON_AddStringToObject(params,"qrcode",qrcode);
	cJSON_AddStringToObject(params,"name",name);
	cJSON_AddStringToObject(params,"type",type);
	cJSON_AddStringToObject(result_root,"method","join_family_request");
	cJSON_AddItemToObject(result_root,"params",params);

	return result_root;
}

//APP dial in control
cJSON * join_family(jrpc_context * ctx, cJSON * params, cJSON *id) {
	cJSON *param = NULL;
	char peeraddress[64];
	memset(peeraddress,0,64);
	int peerport = 0;
	char qrcode[512];
	memset(qrcode,0,512);
	char type[16];
	memset(type,0,16);
	char sqlstr[1024];
	memset(sqlstr,0,1024);
	char name[64];
	memset(name,0,64);
	char version[32];
	memset(version,0,32);
	int ret = 0;
	int fd = ((struct jrpc_connection *)(ctx->data))->fd;
	cJSON *packet42 = NULL;
	char *packet42_str;
	char *tmpstr;
	app_client_t * appt = g_app_client_list;
	char sentrybox[2048];
	bzero(sentrybox,2048);

	char tstr[256];
	int i = 0;
	char cmdstr[2048];

	strcpy(sentrybox,"[");
	for(i=0;i<g_baseconf.sentryboxcount;i++) {
		bzero(tstr,256);
		snprintf(tstr,256,"{\"name\":\"%s\",\"ipaddr\":\"%s\"}",g_baseconf.sentrybox[i].name,g_baseconf.sentrybox[i].ipaddr);
		strncat(sentrybox,tstr,256);
		if(i<g_baseconf.sentryboxcount-1)
			strcat(sentrybox,",");
	}
	strcat(sentrybox,"]");


	peerport = get_peer_address(fd,peeraddress);

	//get_local_address(((struct jrpc_connection *)(ctx->data))->fd);
	if(strlen(g_localaddress) == 0)
		get_local_address();
	if(params == NULL)
		return cJSON_CreateString("join family null params!");

	param = cJSON_GetObjectItem(params,"qrcode");
	if(param != NULL && param->type == cJSON_String ) {
		strcpy(qrcode,param->valuestring);	
	}
	else
		return cJSON_CreateString("family_join_deny");

	//process params
	param = cJSON_GetObjectItem(params, "type");
	if(param != NULL && param->type == cJSON_String )
		strcpy(type,param->valuestring);
	else
		return cJSON_CreateString("family_join_deny");

	param = cJSON_GetObjectItem(params, "name");
	if(param != NULL && param->type == cJSON_String )
		strcpy(name,param->valuestring);

	param = cJSON_GetObjectItem(params, "version");
	if(param != NULL && param->type == cJSON_String )
		strcpy(version,param->valuestring);
	if(strlen(qrcode) > 0) {	
		snprintf(sqlstr,1024,"select * from appinfo where appid='%s'",qrcode);
#ifdef SQL
		ret = query_appinfo_fromdb(sqlstr);
#endif
	}
	//randstr generate
	char randstr[10];
	memset(randstr,0,10);
	
	get_rand_str(randstr,9);
	//init_stream_url();
	//ret = 1; // for test 2017.03.05
	//find the qrcode
	if(ret == 1) {
joinpermit:
		DEBUG_INFO("family_join_permit:%s",peeraddress);
		
		app_client_t* appt = update_app_client_info(fd,peeraddress,peerport,qrcode,JOINED,name,type);	
		strcpy(appt->version,version);

		bzero(appt->audiostream_url,1024);
		bzero(appt->audiostream_out,1024);
		if(strlen(g_baseconf.outerboxip) == 0)
			return cJSON_CreateString("family_join_waiting");
#ifdef ANDROID
		generate_audio_stream(g_baseconf.outerboxip,appt->audiostream_url,randstr);
#else
		generate_audio_stream(g_localaddress,appt->audiostream_url,randstr);
#endif
		generate_audio_stream(g_baseconf.outerboxip,appt->audiostream_out,randstr);

		DEBUG_INFO("#####%s %s",appt->audiostream_url,appt->audiostream_out);	

		bzero(cmdstr,2048);
		snprintf(cmdstr,2048,"{\"result\":\"family_join_permit\",\"audiourl\":\"%s\",\"onekeyset\":\"%d\",\"sentrybox\":%s}^",appt->audiostream_url,g_onekey_set,sentrybox); 		
		write(appt->socket_fd,cmdstr,strlen(cmdstr));
		return NULL;		
		//return cJSON_CreateString("family_join_permit");
	} 
	else
	{
		if(strcmp(type,"android-pad") == 0)
		{
			goto joinpermit;
			//return cJSON_CreateString("family_join_permit");
		}
		else if(strcmp(type,"ios-phone") == 0 || strcmp(type,"android-phone") == 0){
			update_app_client_info(fd,peeraddress,peerport,qrcode,ONLINE,name,type);
			packet42 = packet42_create(qrcode,name,type);
			packet42_str = cJSON_Print(packet42);
			tmpstr = malloc(strlen(packet42_str)+2);
			memset(tmpstr,0,strlen(packet42_str)+2);
			strcpy(tmpstr,packet42_str);
			strncat(tmpstr,"^",1);

			travel_app_list();
			while(appt) {
				char raddress[128];
				bzero(raddress,128);
				int rport = get_peer_address(appt->socket_fd,raddress);
				if(rport != appt->port || strcmp(appt->ip_addr_str,raddress) != 0) {
					appt = appt->next;
					continue;
				}

				if(appt->current_state >= JOINED) {
					//ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
					int lport = get_local_sockinfo(appt->socket_fd);
					if(lport>0 && lport == IN_PORT) {
						ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
					} else
						appt->current_state = OFFLINE;
				}
				if(ret == -1)
					appt->current_state = OFFLINE;

				appt = appt->next;	
			}

			cJSON_Delete(packet42);	
			free(packet42_str);
			free(tmpstr);

			return cJSON_CreateString("family_join_waiting");
			
		} else {
			return cJSON_CreateString("family_join_deny");
		}

	}

}

//pakcet 3 process
cJSON * join_permit(jrpc_context * ctx, cJSON * params, cJSON *id) {
	cJSON * param = NULL;
	app_client_t * appt = g_app_client_list;
	char *packet2  = "{\"result\":\"family_join_permit\"}";
	int ret = 0;

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

	param = cJSON_GetObjectItem(params, "result");
	if(param != NULL && param->type == cJSON_String)
	{
		if(strcmp(param->valuestring,"permit") == 0) {
			param = cJSON_GetObjectItem(params, "qrcode");
			if(param != NULL && param->type == cJSON_String) {
				while(appt) {
					if(strcmp(appt->app_dev_id,param->valuestring) == 0)
					{
						ret = write(appt->socket_fd,packet2,strlen(packet2));
						if(ret == -1)
							appt->current_state = OFFLINE;
						else
							update_app_client_info(appt->socket_fd,NULL,0,appt->app_dev_id,OFFLINE,appt->name,appt->type);			
						break;

					}
					appt = appt->next;
				}
			}

		}
	}
	return cJSON_CreateString("packet2_received");
}

int send_msg_to_server(char * serveraddr,int port, char * cmd)
{
 	int    sockfd, n;
    	char    recvline[4096], sendline[4096];
    	struct sockaddr_in    servaddr;
    	//char *cmd="{\"method\":\"opendoor_permit\"}\0";

    	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        	DEBUG_ERR_LOG("create socket error: %s(errno: %d)", strerror(errno),errno);
        	return 0;
    	}

   	memset(&servaddr, 0, sizeof(servaddr));
   	servaddr.sin_family = AF_INET;
    	servaddr.sin_port = htons(port);
    	if( inet_pton(AF_INET, serveraddr, &servaddr.sin_addr) <= 0){
        	DEBUG_ERR_LOG("inet_pton error for %s",serveraddr);
		close(sockfd);
        	return 0;
    	}

 	unsigned long ul = 1;
       	ioctl(sockfd, FIONBIO, &ul);
	int ret = -1;
	fd_set set;
	struct timeval tm;
	int error = -1, len;
	len = sizeof(int);
	DEBUG_INFO("connect and send to %s %s",serveraddr,cmd);
    	if( connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		tm.tv_sec = 2;
		tm.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(sockfd, &set);
		if(select(sockfd+1, NULL, &set, NULL, &tm) > 0) {
			getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
			if(error == 0) ret = 0;
		}
    	}
	else
		ret = 0;

	if(ret == -1) {
		close(sockfd);
        	DEBUG_ERR_LOG("connect error: %s(errno: %d)",strerror(errno),errno);
        	return 0;
	}
		
	ul = 0;
	ioctl(sockfd, FIONBIO, &ul);

    	//gets(sendline, 4096, stdin);

    	if( send(sockfd, cmd, strlen(cmd), 0) < 0)
    	{
        	DEBUG_ERR_LOG("send msg error: %s(errno: %d)", strerror(errno), errno);
		close(sockfd);
        	return 0;
    	}

        usleep(100000);
        ret = recv(sockfd,recvline,sizeof(recvline),0);
        if(ret <= 0) {
                close(sockfd);
                return 0;
        }
        DEBUG_INFO("sockfd:%d %s %s",sockfd,cmd,recvline);
    	//printf("sockfd:%d %s\n",sockfd,cmd);
    	close(sockfd);
    	return 1;
}

int send_msg_to_outer(char * serveraddr,char * cmd)
{
 	int    sockfd, n;
    	char    recvline[4096], sendline[4096];
    	struct sockaddr_in    servaddr;
    	//char *cmd="{\"method\":\"opendoor_permit\"}\0";

    	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        	DEBUG_ERR_LOG("create socket error: %s(errno: %d)", strerror(errno),errno);
        	return 0;
    	}

   	memset(&servaddr, 0, sizeof(servaddr));
   	servaddr.sin_family = AF_INET;
    	servaddr.sin_port = htons(7890);
    	if( inet_pton(AF_INET, serveraddr, &servaddr.sin_addr) <= 0){
        	DEBUG_ERR_LOG("inet_pton error for %s",serveraddr);
		close(sockfd);
        	return 0;
    	}

 	unsigned long ul = 1;
       	ioctl(sockfd, FIONBIO, &ul);
	int ret = -1;
	fd_set set;
	struct timeval tm;
	int error = -1, len;
	len = sizeof(int);
	DEBUG_INFO("connect and send to %s %s",serveraddr,cmd);
    	if( connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		tm.tv_sec = 2;
		tm.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(sockfd, &set);
		if(select(sockfd+1, NULL, &set, NULL, &tm) > 0) {
			getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
			if(error == 0) ret = 0;
		}
    	}
	else
		ret = 0;

	if(ret == -1) {
        	DEBUG_ERR_LOG("connect error: %s(errno: %d)",strerror(errno),errno);
		close(sockfd);
        	return 0;
	}
		
	ul = 0;
	ioctl(sockfd, FIONBIO, &ul);

    	//gets(sendline, 4096, stdin);

    	if( send(sockfd, cmd, strlen(cmd), 0) < 0)
    	{
        	DEBUG_ERR_LOG("send msg error: %s(errno: %d)", strerror(errno), errno);
		close(sockfd);
        	return 0;
    	}
    	DEBUG_INFO("sockfd:%d %s",sockfd,cmd);
    	close(sockfd);
    	return 1;
}

//when app get door open request,push 接听 按钮时发送消息给室内机
//室内机收到消息时，由该函数处理
cJSON * door_open_processing(jrpc_context * ctx, cJSON * params, cJSON *id) {
	app_client_t * appt = g_app_client_list;
	app_client_t * procer;
	//char *tmpstr = "{\"door_open_state:\":\"processing\"}^";
	char *tmpstr = "{\"method\":\"call_ending\"}^";
	int ret = 0;
	char timestr[32];
	char event[512];

	memset(timestr,0,32);
	memset(event,0,512);

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	char cmd[1024];

	procer = get_appt_by_ctx(ctx);
	
	snprintf(cmd,1024,"{\"method\":\"play_live_audio\",\"params\":{\"streamurl\":\"%s\"}}",procer->audiostream_out);
	
        //ret = send_msg_to_outer(g_baseconf.outerboxip,cmd);
        ret = send_msg_to_outer(procer->line_state->peeripaddr,cmd);
	DEBUG_INFO("door_opend_processing:%s",cmd);

	//maybe outerbox is offline	
	if(ret == 0)
	{
		g_outerbox_online = 0;
		return cJSON_CreateString("outerbox_is_offline");
	}
	g_outerbox_online = 1;
	
	//state machine processing
	update_app_client_info(((struct jrpc_connection*)(ctx->data))->fd, NULL, 0, NULL, CALLING, NULL, NULL);
	procer->current_state = CALLING;

	while(appt) {
		char raddress[128];
		bzero(raddress,128);
		int rport = get_peer_address(appt->socket_fd,raddress);
		if(rport != appt->port || strcmp(appt->ip_addr_str,raddress) != 0) {
			appt = appt->next;
			continue;
		}

		if(appt->current_state >= JOINED && appt->current_state < CALLING) {
			int lport = get_local_sockinfo(appt->socket_fd);
			if(lport>0 && lport == IN_PORT) {
				ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
				DEBUG_INFO("send door openprocessing: %s %s",appt->ip_addr_str,tmpstr);
			} else
				appt->current_state = OFFLINE;
		}	

		if(ret == -1)
			update_app_client_info(appt->socket_fd, NULL, 0, NULL, OFFLINE, NULL, NULL);

		appt = appt->next;	
	}

	//log
	get_time_now(timestr);
	procer = get_appt_by_ctx(ctx);
	//snprintf(event,512,"接听(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
	snprintf(event,512,"接听");
#ifdef LOG	
	DEBUG_INFO("envent:%s",event);
#endif			
	callhistory_log_to_db(timestr,g_last_caller,event);

	//g_line_is_busy = ((struct jrpc_connection*)(ctx->data))->fd;

	return cJSON_CreateString("door_open_processing_has_done");
}

// tempory solutions for beijing chengyuan 
// incomplete elevator guest cmd 
#ifdef BEIJING_ELEVATOR
void * elevator_host_cmd_call() {

	pthread_detach(pthread_self());

	sleep(40);
	
	if(strlen(g_localaddress)==0)
		get_local_address();
	char cmd[1024];
	bzero(cmd,1024);	
	snprintf(cmd,1024,"{\"method\":\"call_elevator\",\"params\":{\"cmd\":2,\"ipaddr\":\"%s\"}}",g_localaddress);
	send_msg_to_outer(g_baseconf.outerboxip,cmd);
	
}
#endif

cJSON * opendoor_permit(jrpc_context * ctx, cJSON * params, cJSON *id) {
	int ret = 0;
	char timestr[32];
	char event[512];
	app_client_t * procer;

	memset(timestr,0,32);
	memset(event,0,512);
	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

	//do something
	char * cmd = "{\"method\":\"opendoor_permit\"}\0";
        //ret = send_msg_to_outer(g_baseconf.outerboxip,cmd);
	procer = get_appt_by_ctx(ctx);
        ret = send_msg_to_outer(procer->line_state->peeripaddr,cmd);
	//maybe outerbox is offline	
	if(ret == 0)
	{
		g_outerbox_online = 0;
		return cJSON_CreateString("outerbox_is_offline");
	}

	if(strlen(g_localaddress) == 0)
		get_local_address();
	//call_elevator
	char cmd_elevator[1024];
	bzero(cmd_elevator,1024);
	snprintf(cmd_elevator,1024, "{\"method\":\"call_elevator\",\"params\":{\"cmd\":1,\"ipaddr\":\"%s\"}}",g_localaddress);
	send_msg_to_outer(g_baseconf.outerboxip,cmd_elevator);

#ifdef BEIJING_ELEVATOR
	create_worker(elevator_host_cmd_call,NULL);
#endif	
	g_outerbox_online = 1;
	//log
	get_time_now(timestr);
	//snprintf(event,512,"开门指令发出(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
	snprintf(event,512,"开门指令发出");
#ifdef LOG	
	DEBUG_INFO("envent:%s",event);
#endif			
	callhistory_log_to_db(timestr,g_last_caller,event);

	g_line_is_busy = 0;

	return cJSON_CreateString("opendoor_permit_has_done");

}

cJSON * call_from_property(jrpc_context * ctx, cJSON * params, cJSON *id) {
	char *linebusy = "{\"result\":\"line_is_busy\"}";
	char tmpstr[2048];
	char audio_str[1024];
	bzero(audio_str,1024);
	bzero(tmpstr,2048);
	int ret = 0;
	int busy = 0;
	int outerport = 0;
	char peeraddress[128];
	bzero(peeraddress,128);
	if(strlen(g_localaddress) == 0)
		get_local_address();
/*
	if(g_line_is_busy > 0) {
		write(((struct jrpc_connection*)(ctx->data))->fd,linebusy,strlen(linebusy));
		return NULL;
	}
*/	
	cJSON * param = cJSON_GetObjectItem(params, "audio");
#ifdef ANDROID
	if(param != NULL && param->type == cJSON_String)
	{
		snprintf(audio_str,1024,"%s",param->valuestring);	
	}
#else
	if(param != NULL && param->type == cJSON_String)
	{
		//snprintf(audio_str,1024,"%s",param->valuestring);	
		snprintf(audio_str,1024,"rtmp://%s:1936/hls/stream",g_localaddress);	
	}
#endif
/*
	snprintf(tmpstr,2048,"{\"method\":\"callin_from_property\",\"params\":{\"audio_from_property\":\"%s\",\"audio_to_property\":\"%s\"}}^",audio_str,g_stream_phoneapp);
*/
#ifdef DEBUG
	DEBUG_INFO(tmpstr);
#endif
	outerport = get_peer_address(((struct jrpc_connection*)(ctx->data))->fd,peeraddress);

	app_client_t * appt = g_app_client_list;
	while(appt) {
		char raddress[128];
		bzero(raddress,128);
		int rport = get_peer_address(appt->socket_fd,raddress);
		if(rport != appt->port || strcmp(appt->ip_addr_str,raddress) != 0) {
			appt->current_state = OFFLINE;
			appt = appt->next;
			continue;
		}

		if(appt->current_state >= JOINED && appt->current_state < CALLING) {
			int lport = get_local_sockinfo(appt->socket_fd);
			if(lport>0 && lport == IN_PORT) {
				snprintf(tmpstr,2048,"{\"method\":\"callin_from_property\",\"params\":{\"audio_from_property\":\"%s\",\"audio_to_property\":\"%s\"}}^",audio_str,appt->audiostream_url);
#ifdef DEBUG
	DEBUG_INFO(tmpstr);
#endif
				ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
				appt->calling_property = ((struct jrpc_connection*)(ctx->data))->fd;
				
				if(ret > 0) {
					update_app_line_state(appt,PROPERTY2APP,outerport,peeraddress,((struct jrpc_connection*)(ctx->data))->fd);
					appt->current_state = CONNECTING;
					busy++;
				}

			} else {
				appt->current_state = OFFLINE;
			}
		}

		if(ret == -1) {
			appt->current_state = OFFLINE;
		}

		appt = appt->next;	
	}
	
	//call
	g_property_socket = ((struct jrpc_connection*)(ctx->data))->fd;

	if(busy == 0)
		return cJSON_CreateString("line_is_busy");

	//write(g_property_socket,linebusy,strlen(linebusy));
	
	//snprintf(tmpstr,2048,"{\"result\":\"call_accept\",\"audio\":\"%s\"}",g_stream_apptoout);
	//write(g_property_socket,tmpstr,strlen(tmpstr));

	char timestr[128];
        bzero(timestr,128);
        char event[128];
        bzero(event,128);
        get_time_now(timestr);
        strcpy(event,"物业呼入");
        //callhistory_log_to_db(timestr,g_baseconf.propertyip,event);
        callhistory_log_to_db(timestr,peeraddress,event);

	return NULL;
}

cJSON * callaccept_from_property(jrpc_context * ctx, cJSON * params, cJSON *id) {
	char result[1024];
	bzero(result,1024);
	char tmpstr[256];
	bzero(tmpstr,256);
	snprintf(tmpstr,256,"{\"method\":\"call_ending\"}^");
	int ret = 0;
	app_client_t * callingapp = NULL;
	// send other CALLING APP 	call_ending
	app_client_t * appt = g_app_client_list;
	while(appt) {
		char raddress[128];
		bzero(raddress,128);
		int rport = get_peer_address(appt->socket_fd,raddress);
		if(rport != appt->port || strcmp(appt->ip_addr_str,raddress) != 0) {
			appt->current_state = OFFLINE;
			appt = appt->next;
			continue;
		}

		if(appt->socket_fd == ((struct jrpc_connection*)(ctx->data))->fd) {
			callingapp = appt;
			appt->current_state = CALLING;
			appt = appt->next;
			continue;
		}

		if(appt->current_state == CONNECTING && appt->calling_property > 0 )  {
			if(strcmp(callingapp->ip_addr_str,appt->ip_addr_str)==0)
			{
				appt->current_state = OFFLINE;
				close(appt->socket_fd);
				appt= appt->next;
				continue;
			}

			int lport = get_local_sockinfo(appt->socket_fd);
			if(lport>0 && lport == IN_PORT) {
				ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
				appt->current_state = IDLE;
				appt->calling_property = 0;
			} else {
				appt->current_state = OFFLINE;
			}
		}

		if(ret == -1) {
			appt->current_state = OFFLINE;
		}

		appt = appt->next;	
	}
	
	//call
	snprintf(result,1024,"{\"result\":\"call_accept\",\"audio\":\"%s\"}",callingapp->audiostream_out);
	DEBUG_INFO("g_stream_apptoout:%s %s",result,g_baseconf.outerboxip);

	write(callingapp->line_state->peersocketfd,result,strlen(result));

        char timestr[128];
        bzero(timestr,128);
        char event[128];
        bzero(event,128);
        get_time_now(timestr);
        app_client_t *procer = get_appt_by_ctx(ctx);
        strcpy(event,"物业呼入-住户接听");
        callhistory_log_to_db(timestr,procer->ip_addr_str,event);
	
	return NULL;	
}

cJSON * call_ending_of_otherinner(jrpc_context * ctx, cJSON * params, cJSON *id) {
	char * callending = "{\"method\":\"call_ending\"}^";
	int ret = 0;

        app_client_t * appt = g_app_client_list;
        while(appt) {
		//if(appt->calling_otherinner>0 && appt->calling_otherinner != appt->socket_fd ) {
		if(appt->line_state->calltype == APP2APP && appt->calling_otherinner>0 && appt->current_state>IDLE) {
			appt->current_state =  IDLE;
			ret = write(appt->socket_fd,callending,strlen(callending));
			appt->calling_otherinner = 0;
			bzero(appt->calling_otherinner_ip,64);
			bzero(appt->calling_otherdoorid,64);
			update_app_line_state(appt,IDLE,0,NULL,0);
 		}
		if(ret == -1)
			appt->current_state = OFFLINE;

		appt = appt->next;
	}
}

void * do_call_ending(app_client_t * appt)
{
	send_msg_to_server(appt->calling_otherinner_ip,5678, "{\"method\":\"call_ending_of_otherinner\"}");
}


cJSON * call_ending_from_app(jrpc_context * ctx, cJSON * params, cJSON *id) {
	g_line_is_busy = 0;
	DEBUG_INFO("in call_ending_from_app");
        app_client_t * appt = g_app_client_list;

        char timestr[128];
        bzero(timestr,128);
        char event[128];
        bzero(event,128);
        get_time_now(timestr);

	char tmpstr[1024];
	bzero(tmpstr,1024);

	if(strlen(g_localaddress)==0)
		get_local_address();
#ifdef ANDROID
	snprintf(tmpstr,1024,"{\"method\":\"call_ending\",\"params\":{\"ipaddr\":\"%s\"}}",g_localaddress);
#else
	snprintf(tmpstr,1024,"{\"method\":\"call_ending\",\"params\":{\"ipaddr\":\"%s\"}}",g_baseconf.outerinterfaceip);
#endif
	
        while(appt) {
		app_client_t *procer = get_appt_by_ctx(ctx);
                if(appt->socket_fd == ((struct jrpc_connection*)(ctx->data))->fd) {
			if(appt->line_state->calltype == APP2PROPERTY || appt->line_state->calltype == PROPERTY2APP){
				DEBUG_INFO("call_ending_from_app:%s %d",appt->line_state->peeripaddr,appt->socket_fd);
				send_msg_to_center(appt->line_state->peeripaddr,tmpstr);
                        	appt->calling_property = 0;
				g_property_socket = 0;

                                if(appt->line_state->calltype == APP2PROPERTY)
                                        strcpy(event,"呼叫物业-住户挂断");
                                else
                                        strcpy(event,"物业呼入-住户挂断");
			}

				
			if(appt->line_state->calltype == APP2APP){
				if(appt->calling_otherinner > 0) {
					create_worker(do_call_ending,appt);
					//send_msg_to_server(appt->calling_otherinner_ip,5678, "{\"method\":\"call_ending_of_otherinner\"}");
                        		appt->calling_otherinner = 0;
					strcpy(event,"呼叫其他住户-住户挂断");
				}
			}

			appt->current_state = IDLE;
			update_app_line_state(appt,IDLE,0,NULL,0);
	//		callhistory_log_to_db(timestr,procer->ip_addr_str,event);

                        break;
                }

                appt = appt->next;
	}

	//return NULL;
	return cJSON_CreateString("call_ending_has_done");
}

cJSON * call_ending_of_timeout_from_out(jrpc_context * ctx, cJSON * params, cJSON *id) {

 	app_client_t * appt = g_app_client_list;
        char * callending = "{\"method\":\"call_ending\"}^";
        int ret = 0;

        if(g_property_socket > 0 ){
//              send_msg_to_center(g_baseconf.propertyip,"{\"method\":\"call_ending\"}");
                g_property_socket = 0;
                appt = g_app_client_list;
                while(appt) {
                        if(appt->current_state == CALLING && appt->calling_property > 0) {
                                ret = write(appt->socket_fd,callending,strlen(callending));
                                appt->calling_property = 0;
                                appt->current_state = IDLE;
                        }

                        if(ret == -1)
                                appt->current_state = OFFLINE;

                        appt = appt->next;
                }

        }	
	return cJSON_CreateString("call_ending_of_timeout_has_done");	
}

cJSON * call_ending_from_out(jrpc_context * ctx, cJSON * params, cJSON *id) {
        app_client_t * appt = g_app_client_list;
	char * callending = "{\"method\":\"call_ending\"}^";
	int ret = 0;
        char timestr[128];
        bzero(timestr,128);
        char event[128];
        bzero(event,128);
        get_time_now(timestr);

//	if(g_property_socket > 0 ){
//		send_msg_to_center(g_baseconf.propertyip,"{\"method\":\"call_ending\"}");
		g_property_socket = 0;
        	appt = g_app_client_list;
        	while(appt) {
			app_client_t *procer = get_appt_by_ctx(ctx);
			if(appt->line_state->calltype == APP2PROPERTY || appt->line_state->calltype == PROPERTY2APP){
				ret = write(appt->socket_fd,callending,strlen(callending));
				appt->calling_property = 0;		
				appt->current_state = IDLE;	
				update_app_line_state(appt,IDLE,0,NULL,0);

                                if(appt->line_state->calltype == APP2PROPERTY)
                                        strcpy(event,"呼叫物业-物业挂断");
                                else
                                        strcpy(event,"物业呼入-物业挂断");
			}

			if(appt->line_state->calltype == APP2OUTER || appt->line_state->calltype == OUTER2APP){
				ret = write(appt->socket_fd,callending,strlen(callending));
				appt->current_state = IDLE;	
				update_app_line_state(appt,IDLE,0,NULL,0);
			}

//			callhistory_log_to_db(timestr,procer->ip_addr_str,event);


			if(ret == -1)
				appt->current_state = OFFLINE;

                	appt = appt->next;
		}
		
//	}
	return cJSON_CreateString("call_ending_has_done");	
}

cJSON * opendoor_deny(jrpc_context * ctx, cJSON * params, cJSON *id) {
	int ret = 0;
	char timestr[32];
	char event[512];
	app_client_t * procer;

	memset(timestr,0,32);
	memset(event,0,512);

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

	procer = get_appt_by_ctx(ctx);
	//state machine processing

	char * cmd = "{\"method\":\"call_ending\"}\0";
        //ret = send_msg_to_outer(g_baseconf.outerboxip,cmd);

        ret = send_msg_to_outer(procer->line_state->peeripaddr,cmd);

	update_app_client_info(((struct jrpc_connection*)(ctx->data))->fd, NULL, 0, NULL, IDLE, NULL, NULL);
	update_app_line_state(procer,IDLE,0,NULL,0);
	//maybe outerbox is offline	
	if(ret == 0)
	{
		g_outerbox_online = 0;
		return cJSON_CreateString("outerbox_is_offline");
	}
	g_outerbox_online = 1;

	//log
	get_time_now(timestr);
	//snprintf(event,512,"挂断(by %s )",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
	snprintf(event,512,"挂断");
#ifdef LOG
	DEBUG_INFO("envent:%s",event);
#endif			
	callhistory_log_to_db(timestr,g_last_caller,event);

	g_line_is_busy = 0;
	return cJSON_CreateString("opendoor_deny_has_done");
}

int get_page_of_callhistory()
{
	char *sqlstr = "select count(*) from callhistory";

	return query_callhistory_count(sqlstr);
}

cJSON * callhistory_get(int page)
{
	char *sqlstr = "select * from callhistory order by datetime(\"calltime\") desc";
	char sqlstr1[1024];
	memset(sqlstr1,0,1024);
	int pages = 0;
	int count = 0;

	snprintf(sqlstr1,1024,"%s limit %d,%d",sqlstr,(page-1)*PAGE_NUMBER,page*PAGE_NUMBER);	
	count = get_page_of_callhistory();

	if(count <= PAGE_NUMBER)
		pages = 1;
	else
		pages = count/PAGE_NUMBER + ((count/PAGE_NUMBER*PAGE_NUMBER)==count?0:1);
	return  query_callhistory_fromdb(sqlstr1,page,pages);
}

int get_page_of_msghistory()
{
	char *sqlstr = "select count(*) from msghistory";

	return query_msghistory_count(sqlstr);
}

cJSON * msghistory_get(int page) {
	
	char *sqlstr = "select * from msghistory order by datetime(\"time\") desc";
	char sqlstr1[1024];
	memset(sqlstr1,0,1024);
	int pages = 0;
	int count = 0;

	snprintf(sqlstr1,1024,"%s limit %d,%d",sqlstr,(page-1)*PAGE_NUMBER,page*PAGE_NUMBER);	
	count = get_page_of_msghistory();

	if(count <= PAGE_NUMBER)
		pages = 1;
	else
		pages = count/PAGE_NUMBER + ((count/PAGE_NUMBER*PAGE_NUMBER)==count?0:1);
	
	return  query_msghistory_fromdb(sqlstr1,page,pages);

}

cJSON * callhistory_request(jrpc_context * ctx, cJSON * params, cJSON *id) {
	
	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	cJSON *page;
	int pgn = 0;

	if(params != NULL && params->type == cJSON_Object) {
		page = cJSON_GetObjectItem(params,"page");
		if(page != NULL && page->type == cJSON_Number){
			pgn = page->valueint;			
		}
	}
			
	cJSON * chj = callhistory_get(pgn);
	return chj;
}

cJSON * msghistory_request(jrpc_context * ctx, cJSON * params, cJSON *id) {

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	cJSON *page;
	int pgn = 0;

	if(params != NULL && params->type == cJSON_Object) {
		page = cJSON_GetObjectItem(params,"page");
		if(page != NULL && page->type == cJSON_Number){
			pgn = page->valueint;			
		}
	}
	
	cJSON * mhj = msghistory_get(pgn);
	return mhj;
}

cJSON * callin_set(jrpc_context * ctx, cJSON * params, cJSON *id) {
	//do call in set
	//
	return cJSON_CreateString("callinset_has_done");
}

cJSON * app_line_is_busy(jrpc_context * ctx, cJSON * params, cJSON *id) {
	//when we enter this function
	//we know the app is just in busy mode
	//so we should send the message to the caller
	//waiting or just close the calling line
	app_client_t * appt = g_app_client_list;
	while(appt) {
		if(appt->socket_fd == ((struct jrpc_connection*)(ctx->data))->fd) {
			
			break;
		}

		appt = appt->next;	
	}

	return cJSON_CreateString("line_is_busy_sent");
}

void * do_callout_to_property(app_client_t *appt) {
	
	char portstr[16];
	bzero(portstr,16);
	snprintf(portstr,16,"%d",PROPERTY_PORT);
	char callingip[64];
	bzero(callingip,64);

	pthread_detach(pthread_self());

	//app_client_t *appt = get_appt_by_ctx(ctx);
	if(strlen(appt->callout_ipaddr)==0) 
        	strcpy(callingip,g_baseconf.callcenterip);
	else {
        	strcpy(callingip,appt->callout_ipaddr);
		bzero(appt->callout_ipaddr,64);
	}

	struct ev_loop *loopinthread =ev_loop_new(0);
       	connect_watcher *c = new_connector(loopinthread,callingip,portstr);
	if(!c) {
		DEBUG_ERR_LOG("cannot connect to property:%s",callingip);
	
		return cJSON_CreateString("cannot_connect_to_property");
	}	

        c->buffer_size = 1500;
        if(!c->buffer)
                c->buffer = malloc(1500);
        memset(c->buffer, 0, 1500);
        c->pos = 0;

        //write command to property 
	//init_stream_url();
	//get_local_address();
	char call_from_innerbox[2048];
	
	if(strlen(g_localaddress)==0)
		get_local_address();
	//printf("appt$$$$$:%d %d\n",appt,ctx);
#ifdef ANDROID
	snprintf(call_from_innerbox,2048,"{\"method\":\"call_from_innerbox\",\"params\":{\"audio\":\"%s\",\"ipaddr\":\"%s\"}}",appt->audiostream_out,g_localaddress);
#else
	snprintf(call_from_innerbox,2048,"{\"method\":\"call_from_innerbox\",\"params\":{\"audio\":\"%s\",\"ipaddr\":\"%s\"}}",appt->audiostream_out,g_baseconf.outerinterfaceip);
#endif
	c->data = call_from_innerbox;
	c->eio.data = c;
	DEBUG_INFO("call_from_innerbox:%s",call_from_innerbox); 
//       send(c->eio.fd,call_from_innerbox,strlen(call_from_innerbox),0);

        //set_nonblocking(c->eio.fd);
 

        if (c) {
                DEBUG_INFO(
                "Trying connection to %s:%d...",callingip,PROPERTY_PORT
                );
		appt->calling_property = appt->socket_fd;
		appt->current_state = CONNECTING;
		update_app_line_state(appt,APP2PROPERTY,PROPERTY_PORT,callingip,c->eio.fd);

                ev_io_start(loopinthread, &c->eio);
		ev_run(loopinthread,0);
        }
        else {
                DEBUG_ERR_LOG( "Can't create a new connection");
                return cJSON_CreateString("Cannot_connect_to_Property");
        }

        char timestr[128];
        bzero(timestr,128);
        char event[128];
        bzero(event,128);
        get_time_now(timestr);
        //app_client_t *procer = get_appt_by_ctx(ctx);
        strcpy(event,"呼叫物业");
        callhistory_log_to_db(timestr,appt->ip_addr_str,event);

	return NULL;
}


cJSON * callout_to_property(jrpc_context * ctx, cJSON * params, cJSON *id) {

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	app_client_t *appt = get_appt_by_ctx(ctx);
	if(appt == NULL)
		return NULL;
	bzero(appt->callout_ipaddr,64);
	DEBUG_INFO("********");
	cJSON * param = NULL;
	if(params != NULL)
		param = cJSON_GetObjectItem(params, "ipaddr");
	if(param != NULL && param->type == cJSON_String)
		snprintf(appt->callout_ipaddr,64,"%s",param->valuestring);	
	else
		snprintf(appt->callout_ipaddr,64,"%s",g_baseconf.callcenterip);
	create_worker(do_callout_to_property,appt);
	return NULL;
}


cJSON * call_outerbox(jrpc_context * ctx, cJSON * params, cJSON *id) {

        //char urlstring[1024];
	int ret =0;

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	
	cJSON *content = cJSON_CreateObject();
	cJSON *result = cJSON_CreateObject();

	app_client_t * appt = get_appt_by_ctx(ctx);

	//sprintf(urlstring,"rtsp://%s:554/ch01_sub.h264",g_localaddress);
	char streamurl[1024];
	bzero(streamurl,1024);

	if(strlen(g_localaddress)==0)
		get_local_address();
        //snprintf(streamurl,1024,"rtsp://admin:admin@%s:554/stream1",g_baseconf.outerboxip);
#ifdef ANDROID
        snprintf(streamurl,1024,"rtsp://admin:admin@%s:554/stream1",g_baseconf.outerboxip);
#else
        snprintf(streamurl,1024,"rtsp://admin:admin@%s:554/stream1",g_localaddress);
#endif

	DEBUG_INFO("call outerbox:%s",streamurl);	
	cJSON_AddStringToObject(content,"video_from_outerbox",streamurl);
        //sprintf(urlstring,"rtmp://%s:1935/live",g_localaddress);
	cJSON_AddStringToObject(content,"audio_to_outerbox",appt->audiostream_url);

        cJSON_AddItemToObject(result,"call_outerbox_params",content);
/*
	char * cmd = "{\"method\":\"play_live_audio\"}\0";
        ret = send_msg_to_outer(g_baseconf.outerboxip,cmd);
	//maybe outerbox is offline	
	if(ret == 0)
	{
		g_outerbox_online = 0;
		return cJSON_CreateString("outerbox_is_offline");
	}
*/
	appt->line_state->calltype = APP2OUTER;		
	update_app_line_state(appt,APP2OUTER,7890,g_baseconf.outerboxip,((struct jrpc_connection*)(ctx->data))->fd);

	return result;	
}

cJSON * call_elevator_inter(jrpc_context * ctx, cJSON * params, cJSON *id) {


	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	char doorid[64];
	bzero(doorid,64);

	cJSON * param = cJSON_GetObjectItem(params, "doorid");
	if(param != NULL && param->type == cJSON_String)
	{
		snprintf(doorid,64,"%s",param->valuestring);	
		DEBUG_INFO("doorid:%s",doorid);
	}
	
	innerbox_info_t * innerbox_t1 = select_from_innerbox_info_list_byid(doorid);
	if(innerbox_t1 == NULL)
		return cJSON_CreateString("call_elevator_inter_wrong_param");

	if(strlen(g_localaddress)==0)
		get_local_address();

	innerbox_info_t * innerbox_t2 = select_from_innerbox_info_list_byip(g_localaddress);
	if(innerbox_t2 == NULL)
		return cJSON_CreateString("call_elevator_inter_wrong_param");
	if(strncmp(innerbox_t1->doorid,innerbox_t2->doorid,6)!=0)
		return cJSON_CreateString("call_elevator_inter_not_same_element");
	
	char cmd[1024];
	bzero(cmd,1024);	
	snprintf(cmd,1024,"{\"method\":\"call_elevator\",\"params\":{\"cmd\":3,\"ipaddr\":\"%s\",\"ipaddr_from\":\"%s\"}}",g_localaddress,innerbox_t1->ipaddr);
	send_msg_to_outer(g_baseconf.outerboxip,cmd);
	DEBUG_INFO("cmd:%s",cmd);
	return cJSON_CreateString("call_elevator_inter_has_done");
}

cJSON * call_elevator(jrpc_context * ctx, cJSON * params, cJSON *id) {

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	
	if(strlen(g_localaddress)==0)
		get_local_address();
	char cmd[1024];
	bzero(cmd,1024);	
	snprintf(cmd,1024,"{\"method\":\"call_elevator\",\"params\":{\"cmd\":2,\"ipaddr\":\"%s\"}}",g_localaddress);
	send_msg_to_outer(g_baseconf.outerboxip,cmd);

	return cJSON_CreateString("call_elevator_has_done");
}

cJSON * call_accept_from_otherinner(jrpc_context * ctx, cJSON * params, cJSON *id) {
	char tmpstr[256];
	bzero(tmpstr,256);
	snprintf(tmpstr,256,"{\"method\":\"call_ending\"}^");
	int ret = 0;
	char resultstr[256];
	bzero(resultstr,256);
	strcpy(resultstr,"{\"result\":\"call_accept_from_otherinner\"}");

	// send other CALLING APP 	call_ending
	app_client_t * appt = g_app_client_list;
	while(appt) {
		char raddress[128];
		bzero(raddress,128);
		int rport = get_peer_address(appt->socket_fd,raddress);
		if(rport != appt->port || strcmp(appt->ip_addr_str,raddress) != 0) {
			appt = appt->next;
			continue;
		}

		if(appt->socket_fd == ((struct jrpc_connection*)(ctx->data))->fd) {
			appt->current_state = CALLING;
			write(appt->calling_otherinner,resultstr,strlen(resultstr));
			appt = appt->next;
			continue;
		}

		if(appt->current_state == CALLING && appt->calling_otherinner > 0 )  {
			int lport = get_local_sockinfo(appt->socket_fd);
			if(lport>0 && lport == IN_PORT) {
				ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
				appt->current_state = IDLE;
				appt->calling_otherinner = 0;
			} else {
				appt->current_state = OFFLINE;
			}
		}

		if(ret == -1) {
			appt->current_state = OFFLINE;
		}

		appt = appt->next;	
	}
	
	//call
	
	return cJSON_CreateString("call_accpet_from_otherinner_cmd_sent");
}


cJSON * call_from_otherinner(jrpc_context * ctx, cJSON * params, cJSON *id) {
	int busy = 0;
	app_client_t * appt = g_app_client_list;
	int ret = 0;
	char tmpstr[2048];
	bzero(tmpstr,2048);
	cJSON *method_root = cJSON_CreateObject();
	cJSON *string = cJSON_CreateString("call_from_otherinner");
	cJSON_AddItemToObject(method_root,"method",string);
	cJSON_AddItemToObject(method_root,"params",params);
	char *jstr = cJSON_Print(method_root);
	strcpy(tmpstr,jstr);
	strncat(tmpstr,"^",1);
	char raddress[128];
	bzero(raddress,128);
	char otherinneraddr[128];
	bzero(otherinneraddr,128);	
	int fd = ((struct jrpc_connection *)(ctx->data))->fd;

	int otherport = get_peer_address(fd,otherinneraddr);

	while(appt) {
		int rport = get_peer_address(appt->socket_fd,raddress);
		if(rport != appt->port || strcmp(appt->ip_addr_str,raddress) != 0) {
			appt = appt->next;
			continue;
		}

		if(appt->current_state >= JOINED && appt->current_state <CALLING) {
			int lport = get_local_sockinfo(appt->socket_fd);
			if(lport>0 && lport == IN_PORT)  {
				busy++;	
				ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
				appt->current_state = CONNECTING;
				appt->calling_otherinner = ((struct jrpc_connection *)(ctx->data))->fd;
				update_app_line_state(appt,APP2APP,otherport,otherinneraddr,appt->calling_otherinner);
				strcpy(appt->calling_otherinner_ip,otherinneraddr);
				DEBUG_INFO("send call_from_otherinner cmd to:%s %d",appt->ip_addr_str,ret);
			} else
				appt->current_state = OFFLINE;
		}
		if(ret == -1) {
			appt->current_state = OFFLINE;
		}

		appt = appt->next;	
	}

	if(busy==0)
		return cJSON_CreateString("line_is_busy");
	else
		return NULL;

}

void * do_otherinner_ipaddr_query_from_property() {
	char cmdstr[2048];
	bzero(cmdstr,2048);

	struct ev_loop *loop_init ;
        loop_init = ev_loop_new(0);
        if (!loop_init) {
                DEBUG_ERR_LOG("Can't initialise libev; bad $LIBEV_FLAGS in environment?");
                exit(1);
        }

	snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_innerbox\",\"params\":{\"ipaddr\":\"%s\"}}",g_baseconf.gateouterboxip);

	//snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_otherinner\",\"params\":{\"doorid\":\"%s\"}}",g_callotherinner_doorid);
	DEBUG_INFO("cmdstr:%s",cmdstr);
	query_info_from_property(cmdstr,loop_init);
        //ev_run(loop_init, 0);
	DEBUG_INFO("%s",cmdstr);
}

void * otherinner_ipaddr_query_from_property() {
	
	pthread_detach(pthread_self());
	
	while(g_innerbox_get_flag == 0) {
		if(strlen(g_baseconf.gateouterboxip) == 0) {
			DEBUG_ERR_LOG("no gate outerbox ip");
			sleep(3);
			continue;
		}
 
		sleep(17);

		do_otherinner_ipaddr_query_from_property();
	}
}

cJSON * call_other_innerbox(jrpc_context * ctx, cJSON * params, cJSON *id) {

	char cmdstr[2048];
	bzero(cmdstr,2048);

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	
	app_client_t * appt = get_appt_by_ctx(ctx);
	bzero(g_callotherinner_doorid,32);
	cJSON * param = cJSON_GetObjectItem(params, "doorid");
	if(param != NULL && param->type == cJSON_String)
	{
		snprintf(g_callotherinner_doorid,32,"%s",param->valuestring);	
	}
	else
		return cJSON_CreateString("wrong_parameters_of_call_other_innerbox");
		
	innerbox_info_t * innerbox_t = select_from_innerbox_info_list_byid(g_callotherinner_doorid);
	DEBUG_INFO("*************doorid:%s",g_callotherinner_doorid);	

	if(innerbox_t == NULL)
		return cJSON_CreateString("wrong_doorid_for_call");

	appt->calling_otherinner = appt->socket_fd;
	strcpy(appt->calling_otherdoorid,g_callotherinner_doorid);
	strcpy(appt->calling_otherinner_ip,innerbox_t->ipaddr);
	//just for test not immplement
	create_worker(do_call_other_innerbox,appt);
	return cJSON_CreateString("call_other_innerbox_request_sent");
}

int get_seczone_conf()
{
    	dictionary *ini ;

    	int   b = 0;
	int   i = 0;
    	double d;
    	char  *s;
	char zonename[64];
	memset(zonename,0,64);

    	ini = iniparser_load(SECZONE_CONF);
    	if (ini == NULL) {
        	fprintf(stderr, "cannot parse file: %s\n", SECZONE_CONF);
        	return -1 ;
    	}

    	s = iniparser_getstring(ini, "password:pass", NULL);
	if(s)
		strncpy(g_seczone_passwd,s,512);


	g_seczone_count = iniparser_getint(ini, "zonenumber:count", 0);
	DEBUG_INFO("#####g_seczone_count:%d",g_seczone_count);	
    	
	g_onekey_set = iniparser_getint(ini, "onekeyset:state", 0);
	DEBUG_INFO("#####g_onekey_set:%d",g_onekey_set);	

	for(i=0; i<g_seczone_count; i++){
		sprintf(zonename,"zone%d:port",i+1);
		b = iniparser_getint(ini,zonename,0);
		g_seczone_conf[i].port = b;					
		
		sprintf(zonename,"zone%d:name",i+1);
		s = iniparser_getstring(ini,zonename,NULL);
		if(s)
			strncpy(g_seczone_conf[i].name,s,128);					

		sprintf(zonename,"zone%d:normalstate",i+1);
		s = iniparser_getstring(ini,zonename,NULL);
		if(s)
			strncpy(g_seczone_conf[i].normalstate,s,16);					

		sprintf(zonename,"zone%d:onekeyset",i+1);
		s = iniparser_getstring(ini,zonename,NULL);
		if(s)
			strncpy(g_seczone_conf[i].onekeyset,s,8);					

		sprintf(zonename,"zone%d:currentstate",i+1);
		s = iniparser_getstring(ini,zonename,NULL);
		if(s)
			strncpy(g_seczone_conf[i].currentstate,s,8);					

		sprintf(zonename,"zone%d:delaytime",i+1);
		b = iniparser_getint(ini,zonename,0);
		g_seczone_conf[i].delaytime = b;	
				
		sprintf(zonename,"zone%d:nursetime",i+1);
		b = iniparser_getint(ini,zonename,0);
		g_seczone_conf[i].nursetime = b;	

		sprintf(zonename,"zone%d:alltime",i+1);
		s = iniparser_getstring(ini,zonename,NULL);
		if(s)
			strncpy(g_seczone_conf[i].alltime,s,8);					
		
		sprintf(zonename,"zone%d:triggertype",i+1);
		g_seczone_conf[i].triggertype = iniparser_getint(ini,zonename,0);
		
		sprintf(zonename,"zone%d:online",i+1);
		s = iniparser_getstring(ini,zonename,NULL);
		if(s)
			strncpy(g_seczone_conf[i].online,s,8);					
	}

    	iniparser_freedict(ini);
    	return 0 ;
}

void write_seczone_conf_file(void)
{
    	FILE *ini;
	int i =0;
    	ini = fopen(SECZONE_CONF, "w");
	fprintf(ini,
    		"#\n"
    		"# secure zone configure file\n"
    		"#\n"
    		"\n"
    		"[password]\n"
    		"pass = %s ;\n"
    		"\n"
    		"\n"
    		"[onekeyset]\n"
    		"state = %d ;\n"
    		"\n"
    		"\n",
		g_seczone_passwd,
		g_onekey_set);

	fprintf(ini,"[zonenumber]\n"
		"count = %d ;\n\n\n",g_seczone_count);

	for(i=0; i<g_seczone_count; i++) {
    		fprintf(ini,
    		"[zone%d]\n"
		"port = %d ;\n"
    		"name = %s ;\n"
		"normalstate = %s ;\n"
		"onekeyset = %s ;\n"
		"currentstate = %s ;\n"
		"delaytime = %d ;\n"
		"nursetime = %d ;\n"
		"alltime = %s ;\n"
		"triggertype = %d ;\n"
		"online = %s ;\n"
    		"\n"
    		"\n",
		i+1,
		g_seczone_conf[i].port,
		g_seczone_conf[i].name,
		g_seczone_conf[i].normalstate,
		g_seczone_conf[i].onekeyset,
		g_seczone_conf[i].currentstate,
		g_seczone_conf[i].delaytime,
		g_seczone_conf[i].nursetime,
		g_seczone_conf[i].alltime,
		g_seczone_conf[i].triggertype,
		g_seczone_conf[i].online
    		);
	}
    fclose(ini);
}

void seczonehistory_log_to_db(char *time,char *type,char *seczone,char *event)
{
	char sqlstr[1024];
	memset(sqlstr,0,1024);
	
	snprintf(sqlstr,1024,"insert into seczonehistory values ('%s', '%s', '%s', '%s')",time, type, seczone, event);
#ifdef SQL
	insert_data_to_db(sqlstr);
#endif
}

cJSON * seczone_pass_set(jrpc_context * ctx, cJSON * params, cJSON *id) {
	//do sec zone password set
	//compare the old pass
	cJSON * oldpass,*newpass;
	app_client_t *procer;
	char event[512];
	char timestr[64];
	memset(event,0,512);
	memset(timestr,0,64);

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

	if(params != NULL && params->type == cJSON_Object) {
		oldpass = cJSON_GetObjectItem(params,"oldpass");
		if(oldpass != NULL && oldpass->type == cJSON_String){
			if(strcmp(g_seczone_passwd,oldpass->valuestring) == 0 || strlen(g_seczone_passwd) == 0) {
				newpass = cJSON_GetObjectItem(params,"newpass");
				if(newpass != NULL && newpass->type == cJSON_String)
					strncpy(g_seczone_passwd,newpass->valuestring,512);
				else
					return cJSON_CreateString("seczone passwd must not be null!");
			}else
				return cJSON_CreateString("wrong old seczone passwd!");

		}else
			return cJSON_CreateString("wrong old seczone passwd!");
			
	}

	//log it 
	get_time_now(timestr);
	procer = get_appt_by_ctx(ctx);
	//snprintf(event,512,"防区密码设置(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
	snprintf(event,512,"防区密码设置");
	seczonehistory_log_to_db(timestr,"设置","防区配置",event);
	
	write_seczone_conf_file();

	return cJSON_CreateString("seczone_pass_set_has_done");
}

void init_seczone_conf_t()
{
	int i = 0;

	for(i=0;i<ZONE_NUMBER;i++){
		g_seczone_conf[i].port = 0;
		memset(g_seczone_conf[i].name,0,128);
		memset(g_seczone_conf[i].normalstate,0,16);
		memset(g_seczone_conf[i].onekeyset,0,8);
		memset(g_seczone_conf[i].currentstate,0,8);
		g_seczone_conf[i].delaytime = 0;
		g_seczone_conf[i].delaycount = 0;
		memset(g_seczone_conf[i].alltime,0,8);
		memset(g_seczone_conf[i].online,0,8);
		g_seczone_conf[i].triggertype = 0;
		g_seczone_conf[i].etime = 0;
	}
	
}

void travel_seczone_conf()
{
	int i = 0;
	DEBUG_INFO("seczone pass:%s",g_seczone_passwd);
	for(i=0;i<g_seczone_count;i++){
		DEBUG_INFO("seczone:%d %s %s",g_seczone_conf[i].port,g_seczone_conf[i].name,g_seczone_conf[i].normalstate);
	}
	
}


cJSON * destroy_emergency(jrpc_context * ctx, cJSON * params, cJSON *id) {
	cJSON *password;
	if(strlen(g_localaddress)==0)
		get_local_address();
	if(params->type == cJSON_Object) {
		password = cJSON_GetObjectItem(params,"password");
		if(password != NULL && password->type == cJSON_String) {
			if(strcmp(password->valuestring,g_seczone_passwd)!=0) 
				return cJSON_CreateString("destroy_emergency_wrong_password");
			else {
				char timestr[128];
				bzero(timestr,128);
				get_time_now(timestr);
				char msgtoproperty[1024];
				bzero(msgtoproperty,1024);
#ifdef ANDROID
				snprintf(msgtoproperty,1024,"{\"method\":\"destroy_emergency\",\"params\":{\"time\":\%s\",\"ipaddr\":\"%s\",\"seczone\":\"\"}}",timestr,g_localaddress);
#else
				snprintf(msgtoproperty,1024,"{\"method\":\"destroy_emergency\",\"params\":{\"time\":\%s\",\"ipaddr\":\"%s\",\"seczone\":\"\"}}",timestr,g_baseconf.outerinterfaceip);
#endif
				//send_msg_to_center(g_baseconf.propertyip,msgtoproperty);
				send_msg_to_center(g_baseconf.alarmcenterip,msgtoproperty);


				return cJSON_CreateString("destroy_emergency_success");
			}
		}else return cJSON_CreateString("destroy_emergency_null_parameters");
	}else return cJSON_CreateString("destroy_emergency_wrong_parameters");

}

cJSON * verify_emergency_password(jrpc_context * ctx, cJSON * params, cJSON *id) {
	cJSON *password;
	if(params->type == cJSON_Object) {
		password = cJSON_GetObjectItem(params,"password");
		if(password != NULL && password->type == cJSON_String) {
			if(strcmp(password->valuestring,g_seczone_passwd)!=0) 
				return cJSON_CreateString("verify_emergency_password_false");
			else {

				return cJSON_CreateString("verify_emergency_password_true");
			}
		}else return cJSON_CreateString("null_params");
	}else return cJSON_CreateString("wrong_parameters");

}

cJSON * seczone_conf_set(jrpc_context * ctx, cJSON * params, cJSON *id) {
	//do sec zone config set
	cJSON *password,*config,*item;
	cJSON *port,*name,*normalstate,*onekeyset,*currentstate,*delaytime,*nursetime,*alltime,*online;
	cJSON *triggertype;
	int arraysize = 0,i = 0;
	app_client_t *procer;
	char event[512];
	char timestr[64];
	memset(event,0,512);
	memset(timestr,0,64);

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

	char * jsonprintstr = cJSON_Print(params);
	DEBUG_INFO("seczone_conf_set params:%s",jsonprintstr);
	free(jsonprintstr);

//	if(params->type == cJSON_Object) {
//		password = cJSON_GetObjectItem(params,"password");
//		if(password != NULL && password->type == cJSON_String) {
			//do conf get
			if(1) {
			DEBUG_INFO("g_seczone_passwd:%s",g_seczone_passwd);
//			if(strcmp(password->valuestring,g_seczone_passwd)==0) {
			if(1) {
				config = cJSON_GetObjectItem(params,"config");
				if(config != NULL && config->type == cJSON_Array) {
					arraysize = cJSON_GetArraySize(config);
					while(i<arraysize) {

						item = cJSON_GetArrayItem(config,i);
						if(item != NULL && item->type == cJSON_Object)
						{
							port = cJSON_GetObjectItem(item,"port");
							/*
							if(port != NULL && port->type == cJSON_Number)
								if(port->valueint <= ZONE_NUMBER && port->valueint > 0)
									g_seczone_conf[port->valueint-1].port = port->valueint;
								else   {
									i++;	
									continue;
								}
							else {
								i++;
								continue;
							}*/

							int k = i;
	
							g_seczone_conf[k].port = k+1;

							name = cJSON_GetObjectItem(item,"name");
							if(name != NULL && name->type == cJSON_String)
								strncpy(g_seczone_conf[k].name, name->valuestring, 128);
							DEBUG_INFO("port->valueint:%d %d",port->valueint,i);	
							normalstate = cJSON_GetObjectItem(item,"normalstate");
							if(normalstate != NULL && normalstate->type == cJSON_String)
								strncpy(g_seczone_conf[k].normalstate, normalstate->valuestring, 16);

							onekeyset = cJSON_GetObjectItem(item,"onekeyset");
							if(onekeyset != NULL && onekeyset->type == cJSON_String)
								strncpy(g_seczone_conf[k].onekeyset, onekeyset->valuestring, 8);

							currentstate = cJSON_GetObjectItem(item,"currentstate");
							if(currentstate != NULL && currentstate->type == cJSON_String)
								strncpy(g_seczone_conf[k].currentstate, currentstate->valuestring, 8);

							delaytime = cJSON_GetObjectItem(item,"delaytime");
							if(delaytime != NULL && delaytime->type == cJSON_Number)
								g_seczone_conf[k].delaytime = delaytime->valueint;
							
							alltime = cJSON_GetObjectItem(item,"alltime");
							if(alltime != NULL && alltime->type == cJSON_String)
								strncpy(g_seczone_conf[k].alltime, alltime->valuestring, 8);

							triggertype = cJSON_GetObjectItem(item,"triggertype");
							if(triggertype != NULL && triggertype->type == cJSON_Number)
								g_seczone_conf[k].triggertype = triggertype->valueint;

							online = cJSON_GetObjectItem(item,"online");
							if(online != NULL && online->type == cJSON_String)
								strncpy(g_seczone_conf[k].online, online->valuestring, 8);
							
							nursetime = cJSON_GetObjectItem(item,"nursetime");
							if(nursetime != NULL && nursetime->type == cJSON_Number)
								g_seczone_conf[k].nursetime = nursetime->valueint;
						}
						i++;

					}
				}
			}else
				return cJSON_CreateString("wrong seczone password");
		}else
			return cJSON_CreateString("wrong seczone password");
/*	}
	else
		return cJSON_CreateString("seczone_conf_set wrong params");
*/			
	if(params)
	{
		travel_seczone_conf();	
		write_seczone_conf_file();
	}

	//log it 
	get_time_now(timestr);
	procer = get_appt_by_ctx(ctx);
	//snprintf(event,512,"防区参数设置(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
	snprintf(event,512,"防区参数设置");
	seczonehistory_log_to_db(timestr,"设置","防区配置",event);

	return cJSON_CreateString("seczone_conf_set_has_done");
}

//packet 16 create
cJSON * seczone_conf_p16_create()
{
	int i = 0;
	cJSON *result_root = cJSON_CreateObject();
	cJSON *content_array = cJSON_CreateArray();
	cJSON *content_obj = NULL;

	for(i=0;i<g_seczone_count;i++){
		
		content_obj = cJSON_CreateObject();	
		cJSON_AddNumberToObject(content_obj,"port",g_seczone_conf[i].port);
		cJSON_AddStringToObject(content_obj,"name",g_seczone_conf[i].name);
		cJSON_AddStringToObject(content_obj,"normalstate",g_seczone_conf[i].normalstate);
		cJSON_AddNumberToObject(content_obj,"triggertype",g_seczone_conf[i].triggertype);
		cJSON_AddStringToObject(content_obj,"onekeyset",g_seczone_conf[i].onekeyset);
		cJSON_AddStringToObject(content_obj,"currentstate",g_seczone_conf[i].currentstate);
		cJSON_AddNumberToObject(content_obj,"delaytime",g_seczone_conf[i].delaytime);
		cJSON_AddNumberToObject(content_obj,"nursetime",g_seczone_conf[i].nursetime);
		cJSON_AddStringToObject(content_obj,"online",g_seczone_conf[i].online);
		cJSON_AddStringToObject(content_obj,"alltime",g_seczone_conf[i].alltime);
		
		cJSON_AddItemToArray(content_array,content_obj);
		
		DEBUG_INFO("seczone: %d %d %d %s %s",ZONE_NUMBER,i,g_seczone_conf[i].port,g_seczone_conf[i].name,g_seczone_conf[i].normalstate);
		
	}

	cJSON_AddItemToObject(result_root,"seczone_conf",content_array);
	
	return result_root;
}

cJSON * seczone_conf_require(jrpc_context * ctx, cJSON * params, cJSON *id) {

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);			
	
	return seczone_conf_p16_create();
}

int send_msg_to_center(char * serveraddr,char * cmd)
{
 	int    sockfd, n;
    	char    recvline[4096], sendline[4096];
    	struct sockaddr_in    servaddr;
    	//char *cmd="{\"method\":\"opendoor_permit\"}\0";

    	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        	DEBUG_ERR_LOG("create socket error: %s(errno: %d)", strerror(errno),errno);
        	return 0;
    	}

   	memset(&servaddr, 0, sizeof(servaddr));
   	servaddr.sin_family = AF_INET;
    	servaddr.sin_port = htons(PROPERTY_PORT);
    	if( inet_pton(AF_INET, serveraddr, &servaddr.sin_addr) <= 0){
		close(sockfd);
        	DEBUG_ERR_LOG("inet_pton error for %s",serveraddr);
        	return 0;
    	}

 	unsigned long ul = 1;
       	ioctl(sockfd, FIONBIO, &ul);
	int ret = -1;
	fd_set set;
	struct timeval tm;
	int error = -1, len;
	len = sizeof(int);

    	if( connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		tm.tv_sec = 2;
		tm.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(sockfd, &set);
		if(select(sockfd+1, NULL, &set, NULL, &tm) > 0) {
			getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
			if(error == 0) ret = 0;
		}
    	}
	else
		ret = 0;

	if(ret == -1) {
        	DEBUG_ERR_LOG("connect error: %s(errno: %d)",strerror(errno),errno);
		close(sockfd);
        	return 0;
	}
		
	ul = 0;
	ioctl(sockfd, FIONBIO, &ul);

    	//gets(sendline, 4096, stdin);

    	if( send(sockfd, cmd, strlen(cmd), 0) < 0)
    	{
        	DEBUG_ERR_LOG("send msg error: %s(errno: %d)", strerror(errno), errno);
		close(sockfd);
        	return 0;
    	}
    	DEBUG_INFO("sended to %s sockfd:%d %s",serveraddr,sockfd,cmd);
    	close(sockfd);
    	return 1;
}

cJSON * seczone_emergency(jrpc_context * ctx, cJSON * params, cJSON *id) {

//	char * msg = "{\"method\":\"seczone_emergency\",\"params\":{\"port\":1,\"name\":\"fire\",\"message\":\"xxxxx\"}}^";
	char msgtoproperty[1024];
	cJSON *root = cJSON_CreateObject();
	int ret = 0;
	cJSON *tmpjson;
	int port = 0;
	char name[64];
	char message[512];
	char timestr[64];
	char seczone[64];
	char msg[512];

	memset(name,0,64);
	memset(message,0,512);
	memset(msg,0,512);
	memset(timestr,0,64);
	memset(seczone,0,64);
	bzero(msgtoproperty,1024);

	if(params != NULL)
	if(params->type == cJSON_Object) {
		tmpjson = cJSON_GetObjectItem(params,"port");
		if(tmpjson != NULL && tmpjson->type == cJSON_Number) 
			port = tmpjson->valueint;
		tmpjson = cJSON_GetObjectItem(params,"name");
		if(tmpjson != NULL && tmpjson->type == cJSON_String) 
			snprintf(name,64,"%s",tmpjson->valuestring);
		tmpjson = cJSON_GetObjectItem(params,"message");
		if(tmpjson != NULL && tmpjson->type == cJSON_String) 
			snprintf(message,512,"%s",tmpjson->valuestring);
	}

	if(strlen(g_localaddress)==0)
		get_local_address();
	if(port == 999){

		get_time_now(timestr);
		seczonehistory_log_to_db(timestr,name,message,message);
#ifdef ANDROID
		snprintf(msgtoproperty,1024,"{\"method\":\"emergency\",\"params\":{\"time\":\"%s\",\"ipaddr\":\"%s\",\"seczone\":\"%s\"}}",timestr,g_localaddress,name);
#else
		snprintf(msgtoproperty,1024,"{\"method\":\"emergency\",\"params\":{\"time\":\"%s\",\"ipaddr\":\"%s\",\"seczone\":\"%s\"}}",timestr,g_baseconf.outerinterfaceip,name);
#endif
		ret = send_msg_to_center(g_baseconf.alarmcenterip,msgtoproperty);
		if(ret == 1)
			return cJSON_CreateString("seczone_emergency_sended");
		else
			return cJSON_CreateString("seczone_emergency_notsend");
	}

	if (port > 0)
	{
		
		//if(strcmp(g_seczone_conf[port-1].currentstate,"on") != 0 && strcmp(g_seczone_conf[port-1].alltime,"yes") != 0 ){
		if((g_onekey_set == 0 || strcmp(g_seczone_conf[port-1].onekeyset,"on") != 0) && strcmp(g_seczone_conf[port-1].alltime,"yes") != 0 ){
			//log to database
			get_time_now(timestr);
			snprintf(seczone,64,"防区:%d-%s",port,g_seczone_conf[port-1].name); 
			snprintf(msg,512,"防区未设防，但传感器触发报警。(%s)",message);
			seczonehistory_log_to_db(timestr,"报警",seczone,msg);
			//return directly
			return cJSON_CreateString("seczone_emergency_notsend");
		}
	}

	cJSON_AddStringToObject(root,"method","seczone_emergency");
	cJSON_AddItemToObject(root,"params",params);
	char * sendstr = cJSON_Print(root);
	char * tmpstr = malloc(strlen(sendstr)+2);
	memset(tmpstr,0,strlen(sendstr)+2);

	strcpy(tmpstr,sendstr);
	strncat(tmpstr,"^",1);

	app_client_t * appt = g_app_client_list;
	while(appt) {
		if(appt->current_state >= JOINED) {
			ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
			DEBUG_INFO("### emergency :%d %s",ret,tmpstr);
		}
		if(ret == -1)
			appt->current_state = OFFLINE;

		appt = appt->next;	
	}

	free(tmpstr);
	free(sendstr);
	//log to database
	get_time_now(timestr);
	snprintf(seczone,64,"防区:%d-%s",port,g_seczone_conf[port-1].name); 
	seczonehistory_log_to_db(timestr,"报警",seczone,message);
#ifdef ANDROID
	snprintf(msgtoproperty,1024,"{\"method\":\"emergency\",\"params\":{\"time\":\"%s\",\"ipaddr\":\"%s\",\"seczone\":\"%s\"}}",timestr,g_localaddress,seczone);
#else
	snprintf(msgtoproperty,1024,"{\"method\":\"emergency\",\"params\":{\"time\":\"%s\",\"ipaddr\":\"%s\",\"seczone\":\"%s\"}}",timestr,g_baseconf.outerinterfaceip,seczone);
#endif
	//ret = send_msg_to_center(g_baseconf.propertyip,msgtoproperty);
	ret = send_msg_to_center(g_baseconf.alarmcenterip,msgtoproperty);
	if(ret==1)
		return cJSON_CreateString("seczone_emergency_sended");
	else
		return cJSON_CreateString("seczone_emergency_notsend");
}

cJSON * seczone_onekey_set(jrpc_context * ctx, cJSON * params, cJSON *id) {

	//do some work
	app_client_t *procer;
	char event[512];
	char timestr[64];
	memset(event,0,512);
	memset(timestr,0,64);
	int i = 0;

        g_onekey_set = 1;
        for(i=0;i<g_seczone_count;i++) {
                if(strcmp(g_seczone_conf[i].onekeyset,"on") == 0)
                        strcpy(g_seczone_conf[i].currentstate,"on");
        }

        write_seczone_conf_file();

	//log it 
	get_time_now(timestr);
	procer = get_appt_by_ctx(ctx);
	//snprintf(event,512,"一键设防(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
	snprintf(event,512,"一键设防");
	seczonehistory_log_to_db(timestr,"设防","所有防区",event);
	return cJSON_CreateString("seczone_onekey_set_has_done");
}

cJSON * seczone_onekey_reset(jrpc_context * ctx, cJSON * params, cJSON *id) {

	cJSON *password;
	//do some work
	int i = 0;

	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

	app_client_t *procer;
	char event[512];
	char timestr[64];
	memset(event,0,512);
	memset(timestr,0,64);

	if(params->type == cJSON_Object) {
		password = cJSON_GetObjectItem(params,"pass");
		if(password != NULL && password->type == cJSON_String) {
			DEBUG_INFO("g_seczone_passwd:%s %s",g_seczone_passwd,password->valuestring);
			if(strcmp(password->valuestring,g_seczone_passwd)==0) {
			//do onekey reset
                                g_onekey_set = 0;
                                for(i=0;i<g_seczone_count;i++){
                			if(strcmp(g_seczone_conf[i].onekeyset,"on") == 0)
                                        	strcpy(g_seczone_conf[i].currentstate,"off");
				}
                                write_seczone_conf_file();
			}
			else
				return cJSON_CreateString("seczone_onekey_reset_wrong_password");

		}
		else
			return cJSON_CreateString("seczone_onekey_reset_wrong_password");
	}
	else
		return cJSON_CreateString("seczone_onekey_reset_wrong_password");

	//log it 
	get_time_now(timestr);
	procer = get_appt_by_ctx(ctx);
	//snprintf(event,512,"一键撤防(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
	snprintf(event,512,"一键撤防");
	seczonehistory_log_to_db(timestr,"撤防","所有防区",event);

	return cJSON_CreateString("seczone_onekey_reset_has_done");
}

int get_page_of_seczonehistory()
{
	char *sqlstr = "select count(*) from seczonehistory";

	return query_seczonehistory_count(sqlstr);
}

cJSON * seczonehistory_get(int page)
{
	char *sqlstr = "select * from seczonehistory order by datetime(\"time\") desc ";
	char sqlstr1[1024];
	memset(sqlstr1,0,1024);
	int pages = 0;
	int count = 0;

	snprintf(sqlstr1,1024,"%s limit %d,%d",sqlstr,(page-1)*PAGE_NUMBER,page*PAGE_NUMBER);	
	count = get_page_of_seczonehistory();

	if(count <= PAGE_NUMBER)
		pages = 1;
	else
		pages = count/PAGE_NUMBER + ((count/PAGE_NUMBER*PAGE_NUMBER)==count?0:1);
	
	return  query_seczonehistory_fromdb(sqlstr1,page,pages);
}

cJSON * seczone_record_history(jrpc_context * ctx, cJSON * params, cJSON *id) {

	if(access_check(ctx) == 0 )
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	cJSON *page;
	int pgn = 0;

	if(params != NULL && params->type == cJSON_Object) {
		page = cJSON_GetObjectItem(params,"page");
		if(page != NULL && page->type == cJSON_Number){
			pgn = page->valueint;			
		}
	}

	return seczonehistory_get(pgn);
}

cJSON * app_update_request(jrpc_context * ctx, cJSON * params, cJSON *id) {
	char sqlstr[1024];
	int ret;
	cJSON *param,*devtype;
	memset(sqlstr,0,1024);

	//check the params
	if(params->type == cJSON_Object) {
		devtype = cJSON_GetObjectItem(params,"devtype");
		if(devtype != NULL && devtype->type == cJSON_String) {
			snprintf(sqlstr,1024,"select * from updateinfo where devtype='%s'",devtype->valuestring);
		}
		else return cJSON_CreateString("app_update_request_wrong_param");
	}
	//read from database
	
#ifdef SQL
	cJSON *app = query_updateinfo_fromdb(sqlstr);
#endif
		
	if(app->child == 0 ) {
		cJSON_Delete(app);
		return cJSON_CreateString("app_update_no_new_version");	
	}

	return app;	
}

void check_outerbox_live2()
{
	g_outerbox_online = send_msg_to_server(g_baseconf.outerboxip,7890, "{\"method\":\"show_version\"}");
}

cJSON * outerbox_netstat(jrpc_context * ctx, cJSON * params, cJSON *id) {
	app_client_t *procer = get_appt_by_ctx(ctx);
	int fd = ((struct jrpc_connection *)(ctx->data))->fd;
	if(access_check(ctx) == 0 ) {
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	}

	check_outerbox_live2();

	if(g_outerbox_online == 0)
		return cJSON_CreateString("outerbox_is_offline");
	else
		return cJSON_CreateString("outerbox_is_online");	
}

void msghistory_log_to_db(char *time,char *url)
{
	char sqlstr[1024];
	memset(sqlstr,0,1024);
	
	snprintf(sqlstr,1024,"insert into msghistory values ('%s','%s')",time, url);
#ifdef SQL
	insert_data_to_db(sqlstr);
#endif
}

//留言记录
//30秒
cJSON * video_record(jrpc_context * ctx, cJSON * params, cJSON *id) {

	char cmdstr[1024];
	memset(cmdstr,0,1024);
	time_t now;
	char rtspname[64];
	memset(rtspname,0,64);
	char timestr[64];
	memset(timestr,0,64);

	snprintf(rtspname,64,"%d.mp4",(int)time(&now));
	snprintf(cmdstr,1024,"ffmpeg -i %s -t 30 -vcodec copy %s%s &",g_stream_outerbox,MSGHISTORY_PATH,rtspname);
#ifdef LOG
	DEBUG_INFO("video_record:%s",cmdstr);
#endif
	system(cmdstr);
		
	get_time_now(timestr);
	msghistory_log_to_db(timestr,rtspname);
	return cJSON_CreateString("video_record_has_done");
}

cJSON * softupdate_query(jrpc_context * ctx, cJSON * params, cJSON *id) {

        char cmdstr[1024];
	if(strlen(g_localaddress) == 0)
		get_local_address();
        snprintf(cmdstr,1024,"{\"method\":\"softupdate_query\",\"params\":{\"version\":\"%s\",\"ipaddr\":\"%s\"}}",g_baseconf.version,g_localaddress);
        send_msg_to_server(g_baseconf.propertyip,18699, cmdstr);

        return cJSON_CreateString(g_baseconf.version);
}

cJSON * ipconf_updated(jrpc_context * ctx, cJSON * params, cJSON *id) {
	create_worker(ipaddr_query_from_property,0);
	
	return NULL;
}

cJSON * start_shell(jrpc_context * ctx, cJSON * params, cJSON *id) {
#ifdef ANDROID
//	system("busybox pkill -9 nc");
#ifdef YANGCHUANG
	system("busybox nc -l -p 1234 -e /system/bin/sh &");
#else
	system("busybox nc -l -p 1234 -e sh &");
#endif
	return cJSON_CreateString("shell started connect to port 1234");
#else
#endif
	return NULL;
}

cJSON * show_version(jrpc_context * ctx, cJSON * params, cJSON *id) {

	char version[128];	
	bzero(version,128);
	int count = 0;
	app_client_t * appt = g_app_client_list;
	while(appt) {
		if(appt->current_state >= JOINED) {
			if(strlen(appt->version) > 0)	{
				if(strlen(version) == 0) {
					strncpy(version,appt->version,16);	
					goto next;
				}

				if(strstr(version,appt->version)==NULL) {
					strcat(version,"+");
					strncat(version,appt->version,16);
					if(count++>2)
						break;
				} 
				
			}
		}
next:
		appt = appt->next;	
	}

	char version_a[256];
	bzero(version_a,256);
	if(strlen(version)>0) {
		snprintf(version_a,256,"%s;APP:%s",g_baseconf.version,version);
		return cJSON_CreateString(version_a);
	}
	else
		return cJSON_CreateString(g_baseconf.version);
}

cJSON * exit_server(jrpc_context * ctx, cJSON * params, cJSON *id) {
	jrpc_server_stop(&my_in_server);
	return cJSON_CreateString("Bye!");
}

#define MONTH_PER_YEAR   12   // 一年12月  
#define YEAR_MONTH_DAY   20   // 年月日缓存大小  
#define HOUR_MINUTES_SEC 20   // 时分秒缓存大小  

int get_compile_time(char * compiletime)
{
  	const char year_month[MONTH_PER_YEAR][4] =
  	{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  	char compile_date[YEAR_MONTH_DAY] = {0}, compile_time[HOUR_MINUTES_SEC] = {0}, i;
  	char str_month[4] = {0};
  	int year, month, day, hour, minutes, seconds;

  	sprintf(compile_date, "%s", __DATE__); // "Aug 23 2016"  
  	sprintf(compile_time, "%s", __TIME__); // "10:59:19"  

  	sscanf(compile_date, "%s %d %d", str_month, &day, &year);
  	sscanf(compile_time, "%d:%d:%d", &hour, &minutes, &seconds);

  	for(i = 0; i < MONTH_PER_YEAR; ++i)
  	{
    		if(strncmp(str_month, year_month[i], 3) == 0)
    		{
      			month = i + 1;
      			break;
    		}
  	}

  	DEBUG_INFO("Compile time is = %d-%.2d-%.2d %.2d:%.2d:%.2d", year, month, day, hour, minutes, seconds);
  	sprintf(compiletime,"%d%.2d%.2d", year, month, day);
	return hour;
}

void get_sys_version()
{

	char compiletime[16];
	bzero(compiletime,16);
	int hour = get_compile_time(compiletime);
	snprintf(g_baseconf.version,64,VERSION,compiletime,hour);	

}
//read content from basic ini file
void get_basic_conf()
{
	bzero(&g_baseconf,sizeof(g_baseconf));
	/*
	memset(g_baseconf.outerboxip,64,0);
	memset(g_baseconf.outerinterfaceip,64,0);
	memset(g_baseconf.propertyip,64,0);
	memset(g_baseconf.familyip,64,0);
	memset(g_baseconf.version,64,0);
	*/
	get_baseconf_fromini(BASIC_CONF);
//	get_version_fromini(VERSION_CONF);
	get_sys_version();
}


void check_outerbox_live()
{
	int s,len;
	struct sockaddr_in addr;
	int addr_len = sizeof(struct sockaddr_in);
	char buffer[1024];
	
	memset(buffer,0,1024);

	if((s = socket(AF_INET,SOCK_DGRAM,0)) < 0){
		perror("udp socket");
		return;
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(HEARTBEAT_PORT);
	addr.sin_addr.s_addr = inet_addr(g_baseconf.outerboxip);

	snprintf(buffer,1024,"{\"method\":\"heartbeat\"}");

	set_nonblocking(s);

	sendto(s,buffer,strlen(buffer),0,(struct sockaddr *) &addr, addr_len);

	bzero(buffer,1024);
	usleep(1000);
	len = recvfrom(s,buffer,sizeof(buffer),0,(struct sockaddr *)&addr,&addr_len);
	
	if(strcmp(buffer,"{\"method\":\"heartbeat\"}") == 0)
		g_outerbox_online = 1;
	else
		g_outerbox_online = 0;

	if(len > 0)
		g_outerbox_online = 1;

	DEBUG_INFO("receive :%s %d",buffer,len);
	close(s);
}

void heartbeat_cb(struct ev_loop *loop, ev_periodic *w, int revents)
{

	DEBUG_INFO("heartbeat 30s ....");	
	int s,len;
	struct sockaddr_in addr;
	int addr_len = sizeof(struct sockaddr_in);
	char buffer[1024];
	char appstate[16];
	
	memset(buffer,0,1024);
	bzero(appstate,16);

	if((s = socket(AF_INET,SOCK_DGRAM,0)) < 0){
		perror("udp socket");
		return;
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(HEARTBEAT_PORT);
	addr.sin_addr.s_addr = inet_addr(g_baseconf.propertyip);

	app_client_t * appt = g_app_client_list;
	strcpy(appstate,"offline");
	while(appt != NULL)
	{
		if(appt->current_state >= JOINED){
			strcpy(appstate,"online");
			break;	
		}
		appt = appt->next;
	}

	if(strlen(appstate) == 0)
		strcpy(appstate,"offline");

	if(strlen(g_localaddress) == 0)
		get_local_address();
	snprintf(buffer,1024,"{\"method\":\"heartbeat\",\"params\":{\"ipaddr\":\"%s\",\"appstate\":\"%s\"}}",g_localaddress,appstate);

	sendto(s,buffer,strlen(buffer),0,(struct sockaddr *) &addr, addr_len);
	
	close(s);

	check_outerbox_live2();
}

//using udp
void * heartbeat_to_property()
{

	pthread_detach(pthread_self());	

        struct ev_loop *loop_tick = ev_loop_new(0);
        ev_periodic heartbeat_tick;
        ev_periodic_init(&heartbeat_tick, heartbeat_cb, 0., 300.,0);
        ev_periodic_start(loop_tick, &heartbeat_tick);
	ev_run(loop_tick,0);

}
/*heatbeat client end*/

void query_info_from_property(char *cmdstr,struct ev_loop *loop)
{

	//char cmdstr[2048];	
	//close(g_connectout_watcher.eio.fd);
	char portstr[16];
	bzero(portstr,16);
	snprintf(portstr,16,"%d",PROPERTY_PORT);
	connect_watcher *c = new_connector(loop,g_baseconf.propertyip, portstr);
	if(!c) {
		DEBUG_ERR_LOG("Cannot connect to property server");
		//ev_loop_destroy(loop);
		return;
	}
	//memset(&g_connectout_watcher,0,sizeof(struct connect_watcher));
	c->buffer_size = 1500;
	if(!c->buffer)
		c->buffer = malloc(1500);
	memset(c->buffer, 0, 1500);
	c->pos = 0;
/*
	bzero(cmdstr,2048);
	snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_outerbox\",\"params\":{\"ipaddr\":\"%s\"}}",g_localaddress);
*/
	//write command to property 
// 	send(c->eio.fd,cmdstr,strlen(cmdstr),0);
	//set_nonblocking(c->eio.fd);
	c->data = cmdstr;
	c->eio.data = c;
 
	if (c) {
		DEBUG_INFO( 
		"Trying connection to %s:%d...",g_baseconf.propertyip,PROPERTY_PORT
		);
		ev_io_start(loop, &c->eio);
        	ev_run(loop, 0);
	}
	else {
	       	DEBUG_ERR_LOG( "Can't create a new connection");
		return;
	}

}


static void create_worker(void *(*func)(), void *arg) {
    	pthread_t       thread;
    	pthread_attr_t  attr;
    	int             ret;

    	pthread_attr_init(&attr);

    	if ((ret = pthread_create(&thread, &attr, func, arg)) != 0) {
        	fprintf(stderr, "Can't create thread: %s\n",
                strerror(ret));
      		return;
    	}
}

void * do_ipaddr_query_from_property() {
	char cmdstr[2048];
	bzero(cmdstr,2048);
	//sleep(30);
	struct ev_loop *loop_init ;
        loop_init = ev_loop_new(0);
        if (!loop_init) {
                DEBUG_ERR_LOG("Can't initialise libev; bad $LIBEV_FLAGS in environment?");
                exit(1);
        }
	
	if(strlen(g_localaddress) == 0)
		get_local_address();

#ifdef ANDROID
	snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_outerbox\",\"params\":{\"ipaddr\":\"%s\"}}",g_localaddress);
#else
	snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_outerbox\",\"params\":{\"ipaddr\":\"%s\"}}",g_baseconf.outerinterfaceip);
#endif
	query_info_from_property(cmdstr,loop_init);
        //ev_run(loop_init, 0);
	DEBUG_INFO("%s",cmdstr);
}

void * ipaddr_query_from_property() {

	pthread_detach(pthread_self());

	while(g_ipconf_get_flag ==0) {
		sleep(13);
		do_ipaddr_query_from_property();
	}
	
}

void execute_cmd(const char *cmd, char *result)
{
    char buf_ps[1024];
    char ps[1024]={0};
    FILE *ptr;
    strcpy(ps, cmd);
    if((ptr=popen(ps, "r"))!=NULL)
    {
        while(fgets(buf_ps, 1024, ptr)!=NULL)
        {
           strcat(result, buf_ps);
           if(strlen(result)>1024)
               break;
        }
        pclose(ptr);
        ptr = NULL;
    }
    else
    {
        printf("popen %s error\n", ps);
    }
}


int send_cmd_to_local(char * serveraddr,char * cmd)
{
 	int    sockfd, n;
    	char    recvline[4096], sendline[4096];
    	struct sockaddr_in    servaddr;
    	//char *cmd="{\"method\":\"opendoor_permit\"}\0";

    	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        	DEBUG_ERR_LOG("create socket error: %s(errno: %d)", strerror(errno),errno);
        	return 0;
    	}

	DEBUG_INFO("cmd:%s",cmd);
	
   	memset(&servaddr, 0, sizeof(servaddr));
   	servaddr.sin_family = AF_INET;
    	servaddr.sin_port = htons(6789);
    	if( inet_pton(AF_INET, serveraddr, &servaddr.sin_addr) <= 0){
        	DEBUG_ERR_LOG("inet_pton error for %s",serveraddr);
        	return 0;
    	}

        struct timeval timeout={5,0};//3s
        setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
        setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));

 	unsigned long ul = 1;
    //   	ioctl(sockfd, FIONBIO, &ul);
	int ret = -1;
	fd_set set;
	struct timeval tm;
	int error = -1, len;
	len = sizeof(int);

    	if( connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		tm.tv_sec = 2;
		tm.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(sockfd, &set);
		if(select(sockfd+1, NULL, &set, NULL, &tm) > 0) {
			getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
			if(error == 0) ret = 0;
		}
    	}
	else
		ret = 0;
	if(errno == 115)
		usleep(100);	
	if(ret == -1) {
        	DEBUG_ERR_LOG("connect error: %s(errno: %d)",strerror(errno),errno);
        	return 0;
	}
		
	ul = 0;
//	ioctl(sockfd, FIONBIO, &ul);

    	//gets(sendline, 4096, stdin);

    	if( send(sockfd, cmd, strlen(cmd), 0) < 0)
    	{
        	DEBUG_ERR_LOG("send msg error: %s(errno: %d)", strerror(errno), errno);
        	return 0;
    	}
    	DEBUG_INFO("sockfd:%d %s",sockfd,cmd);
        usleep(100000);
        ret = recv(sockfd,recvline,sizeof(recvline),0);
        if(ret <= 0) {
                close(sockfd);
                return 0;
        }
    	close(sockfd);
    	return 1;
}

void * server_watchdog()
{

	pthread_detach(pthread_self());
	int pid = getpid();

        char cmd[256];
        bzero(cmd,256);

#ifdef ANDROID
        sprintf(cmd,"busybox ls -l /proc/%d/fd|busybox wc -l",pid);
#else
        sprintf(cmd,"ls -l /proc/%d/fd|wc -l",pid);
#endif
        char result[1024];

	while(1) {
		sleep(30);

                bzero(result,1024);
                execute_cmd(cmd,result);
		printf("$$$$$$$$$$$$$:%d\n",atoi(result));
		if(atoi(result) > 1000 || send_cmd_to_local("127.0.0.1","{\"method\":\"show_version\"}") ==0 ) {
		//if(atoi(result) > 1000 ) {
			DEBUG_ERR_LOG("service error & restart ");
			kill(pid,9);	
		}
	}
}

#ifdef ANDROID

void *app_watchdog()
{

	pthread_detach(pthread_self());

	int count = 0;
	while(1) {
		int lport = 0;
		int online = 0;
 		app_client_t * appt = g_app_client_list;
        	while(appt != NULL) {
			lport = get_local_sockinfo(appt->socket_fd);
			if(lport < 0 || lport != IN_PORT)
				appt->current_state = OFFLINE;
			if(appt->current_state >= ONLINE){	
				online++;
				if(appt->current_state >= CALLING)
					appt->state_count++;
				else
					appt->state_count = 0;

				if(appt->state_count > 4) {
					appt->state_count = 0;
					appt->current_state = ONLINE;
				}
				count = 0;
			}

                	appt = appt->next;
        	}

		if(online == 0) {
			count++;
			if(((count > 5) && (g_update_state == 0)) || (count >10) )
				system("reboot");
		}
			
		sleep(30);
	}
}

/*heartbeat client start*/
void check_nurse_time()
{
	int i = 0,j=0;
	time_t now = 0;
	char cmdstr[1024];

	for(i=0;i<g_seczone_count;i++) {
		if(g_seczone_conf[i].nursetime > 0) {
			time(&now);
			if(g_last_nursetime == 0)
				continue;
			DEBUG_INFO("$$$$$$ nurse:%d %d",now,g_last_nursetime);
			if(now-g_last_nursetime > g_seczone_conf[i].nursetime*60) {
				bzero(cmdstr,1024);
				j=i+1;
				snprintf(cmdstr,1024,"{\"method\":\"seczone_emergency\",\"params\":{\"port\":%d,\"name\":\"%s\",\"message\":\"安防报警\"}}",j,g_seczone_conf[i].name);
				send_cmd_to_local("127.0.0.1",cmdstr);
			}
		}
	}

}


/*android gpio process*/
#ifdef MAICHONG

void *get_gpio_status()
{

	pthread_detach(pthread_self());	

	int i;	
	am335x_gpio_arg arg[8];	
	am335x_gpio_arg arg_oe;   
	//gpio总开关	
	int fd = -1;	
	int Alarm_data[8] = {0};  
	time_t now;
	//这个数组保存的是上层应用设置的报警电平											
	//1 表示高电平触发报警，0 表示低电平触发报警			
	fd = open(DEV_GPIO, O_RDWR);	
	if (fd < 0) {		
		DEBUG_ERR_LOG("Error device open fail! %d", fd);		
		return ;	
	}
	//初始化gpio	
	//for(i=0;i<g_seczone_count;i++){		
	for(i=0;i<8;i++){		
		if(i<4) {			
			arg[i].pin = GPIO_TO_PIN('E', i+14);		
		}		
		else {			
			arg[i].pin = GPIO_TO_PIN('E', i-4);	
		}		
		DEBUG_INFO("arg.pin=%d", arg[i].pin);		
		ioctl(fd, IOCTL_GPIO_SETINPUT, &arg[i]);	
	}		

	//打开gpio总开关	
	arg_oe.pin = GPIO_TO_PIN('E', 11);	
	ioctl(fd, IOCTL_GPIO_SETOUTPUT, &arg_oe);	
	arg_oe.data = 0;	
	ioctl(fd, IOCTL_GPIO_SETVALUE, &arg_oe);			
	DEBUG_INFO("begin to read gpio");	
	char cmdstr[1024];
	int j = 0;
	while(1) {			
		for(i=0;i<g_seczone_count;i++){			
			//迈冲版本的GPIO口顺序是反的
			ioctl(fd, IOCTL_GPIO_GETVALUE, &arg[i]);			
			DEBUG_INFO("gpionum=%d,val=%d\n", i,arg[i].data);			
			//if(arg[i].data != Alarm_data[i]) {				
			if(arg[i].data == g_seczone_conf[7-i].triggertype) {
				DEBUG_INFO("gpio %d is alarm\n",i);
			
#ifdef NURSE
				if(g_seczone_conf[7-i].nursetime > 0){		
					time(&now);
					g_last_nursetime = now;
					continue;
				}
#endif 
				time(&now);
				if(g_seczone_conf[7-i].delaytime > 0) {
					g_seczone_conf[7-i].delaycount++;
					DEBUG_INFO("######delaycount%d %d %d",g_seczone_conf[7-i].delaycount,g_seczone_conf[7-i].delaytime,g_seczone_conf[7-i].etime);
					if((now - g_seczone_conf[7-i].etime) < g_seczone_conf[7-i].delaytime || g_seczone_conf[7-i].delaycount == 1) {
						if(g_seczone_conf[7-i].delaycount == 1)
							g_seczone_conf[7-i].etime = now;
						continue;
					}
				}
				
				if(g_seczone_conf[7-i].delaytime > 0) 
					g_seczone_conf[7-i].etime  = 0;

				bzero(cmdstr,1024);
				j=8-i;
				time(&now);
				DEBUG_INFO("######%d %d",now,g_seczone_conf[7-i].etime);
				if(now-g_seczone_conf[7-i].etime > 10){
					snprintf(cmdstr,1024,"{\"method\":\"seczone_emergency\",\"params\":{\"port\":%d,\"name\":\"%s\",\"message\":\"安防报警\"}}",j,g_seczone_conf[7-i].name);
					
					send_cmd_to_local("127.0.0.1",cmdstr);
					g_seczone_conf[7-i].etime = now;
				}

			} else {		

				if(g_seczone_conf[7-i].delaytime > 0) 
					g_seczone_conf[7-i].delaycount = 0;
			}
		}		
#ifdef NURSE
		check_nurse_time();
#endif
		sleep(1);	
	}	
	close(fd);

}


/* 打开看门狗 */
int watchdogfd = -1;
int watchdog_open(){
	watchdogfd = open("/dev/watchdog", O_RDWR);
	if (watchdogfd < 0) {
		printf("Unable to open /dev/watchdog\n");
	}
	return watchdogfd;
}

/* 关闭看门狗 */

int watchdog_close(){
	int flags = WDIOS_DISABLECARD;
	int result = ioctl(watchdogfd, WDIOC_SETOPTIONS, &flags);
	if (result < 0) {
		 printf("Unable to stop watchdog\n"); 
	}
	close(watchdogfd);
	return result;
}

/* 喂狗函数 */
int watchdog_kick(){
	int result = 0;
	result = ioctl(watchdogfd, WDIOC_KEEPALIVE);
	
	if (result < 0)
	{
		printf("Unable to kick watchdog\n");
	}
	return result;
}

void *watchdog_feeder()
{
	pthread_detach(pthread_self());
	watchdog_open();
        while(1) {
		watchdog_kick();
		sleep(5);
	}
}

#endif

#ifdef YANGCHUANG

#define GPIO_MAX_NUM 32
#define GPIO_TOP_LEVEL GPIO_MAX_NUM-2
#define GPIO_LOW_LEVEL 2


void *watchdog_feeder()
{

	pthread_detach(pthread_self());

	SetWDog(10);
	StartWDog();
	int count = 0;
        while(1) {
		FeedWDog();
                sleep(5);
        }
}



void *get_gpio_status(char * index)
{

	pthread_detach(pthread_self());	

	int ret;
	char cmdstr[1024];
	int i =0;
	int j = 0;
	int tmp = 0;
	time_t now = 0 ;

	//SetIoMode(i,IO_INTR_MODE,g_seczone_conf[i].triggertype);
	while(1) {

		for(i=0;i<g_seczone_count;i++) {

			ret = GetIO(i,IO_POLLING_MODE);
			if(ret == g_seczone_conf[i].triggertype) {
#ifdef NURSE
				if(g_seczone_conf[i].nursetime > 0){		
					time(&now);
					g_last_nursetime = now;
					continue;
				}
#endif 

				time(&now);
				if(g_seczone_conf[i].delaytime > 0) {
					g_seczone_conf[i].delaycount++;
					if((now - g_seczone_conf[i].etime) < g_seczone_conf[i].delaytime || g_seczone_conf[i].delaycount == 1) {
						g_seczone_conf[i].etime = now;
						continue;
					}
				}
				
				if(g_seczone_conf[i].delaytime > 0) 
					g_seczone_conf[i].etime  = 0;


				DEBUG_INFO("#######GetIO %d returned %d %d",i,ret,g_seczone_conf[i].triggertype);
				DEBUG_INFO("gpio %d is alarm",i);
				bzero(cmdstr,1024);
				j=i+1;
				time(&now);
				if(now-g_seczone_conf[i].etime > 10){
					snprintf(cmdstr,1024,"{\"method\":\"seczone_emergency\",\"params\":{\"port\":%d,\"name\":\"%s\",\"message\":\"安防报警\"}}",j,g_seczone_conf[i].name);
					send_cmd_to_local("127.0.0.1",cmdstr);
					g_seczone_conf[i].etime = now;
				}
				
			} else {

				if(g_seczone_conf[i].delaytime > 0) 
					g_seczone_conf[i].delaycount = 0;
	
			}
		}
#ifdef NURSE
		check_nurse_time();
#endif
		usleep(500000);
/*
		printf("start to checking gpio%d...\n",i);
		ret = GetIO(i,IO_INTR_MODE);
		tmp = 0;
		for(j=0;j<GPIO_MAX_NUM;j++){
			tmp += GetIO(i,IO_POLLING_MODE);	
		}

		printf("gpio%d return :%x\n",i,tmp);
		if((tmp >= GPIO_TOP_LEVEL) || (tmp <= GPIO_LOW_LEVEL))	{
			bzero(cmdstr,1024);
			snprintf(cmdstr,1024,"{\"method\":\"seczone_emergency\",\"params\":{\"port\":%d,\"name\":\"%s\",\"message\":\"安防报警\"}}",i+1,g_seczone_conf[i].name);
			send_cmd_to_local("127.0.0.1",cmdstr);
		}
*/
	}

}
#endif

#endif

unsigned long get_file_size(const char *path)
{
        unsigned long filesize = -1;
        struct stat statbuff;
        if(stat(path, &statbuff) < 0) {
                return filesize;
        }else{
                filesize = statbuff.st_size;
        }
        return filesize;
}

void * clear_sqlite_by_datetime()
{
        sqlite3 *db = 0;
        char *pErrMsg = 0;
        int ret = 0;
        char *sSQL1 = "delete from callhistory where calltime <= datetime('now','-365 day')";
        char *sSQL2 = "delete from msghistory where time <= datetime('now','-30 day')";
        char *sSQL3 = "delete from seczonehistory where time <= datetime('now','-365 day')";
        //连接数据库  
        ret = sqlite3_open(INNER_DB, &db);
        if (ret != SQLITE_OK)
        {
                fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                return NULL;
        }

        ret = sqlite3_exec(db, sSQL1, 0, 0, &pErrMsg);
        if (ret != SQLITE_OK)
        {
                fprintf(stderr, "SQL create error: %s\n", pErrMsg);
                sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！  
        }

        ret = sqlite3_exec(db, sSQL2, 0, 0, &pErrMsg);
        if (ret != SQLITE_OK)
        {
                fprintf(stderr, "SQL create error: %s\n", pErrMsg);
                sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！  
        }

        ret = sqlite3_exec(db, sSQL3, 0, 0, &pErrMsg);
        if (ret != SQLITE_OK)
        {
                fprintf(stderr, "SQL create error: %s\n", pErrMsg);
                sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！  
        }

        sqlite3_close(db);
}

void remove_mp4()
{
	char cmdstr[1024];
	memset(cmdstr,0,1024);
	snprintf("rm -rf %s*.mp4",1024,MSGHISTORY_PATH);
	system(cmdstr);
}

void *check_innerdb()
{
	pthread_detach(pthread_self());
        while(1) {
#ifdef ANDROID
                if(get_file_size(INNER_DB) >= 888*1024*1024)
#else
		if(get_file_size(INNER_DB) >= 64*1024*1024)
#endif
                        clear_sqlite_by_datetime();
                sleep(30*24*3600);
        }
}

int main(void) {
	int pid= getpid();
	char cmdstr[2048];
	int i = 0;
	FILE * file = fopen(PID_PATH,"w");
	if(file){
		fprintf(file,"%d",pid);
		fclose(file);
	}

	memset(g_seczone_passwd,0,512);
	memset(g_localaddress,0,64);
	bzero(cmdstr,2048);
	bzero(g_call_from_innerbox,2048);

	init_seczone_conf_t();
	get_basic_conf();
	get_seczone_conf();
	travel_seczone_conf();
	strcpy(g_last_callin_address,g_baseconf.outerboxip);
	
	get_local_address();
	create_basic_conf_file();
	//get basic conf from ini file

        read_innerbox_info_from_conf();
        //travel_innerbox_infolist();
#ifdef ANDROID
        snprintf(g_stream_outerbox,1024,"rtsp://admin:admin@%s:554/stream1",g_last_callin_address);
#endif
	
#ifdef SQL
	create_sqlite_table();
#endif
	//insert_appinfo_to_db("insert into appinfo values('123456','ddd','2017-03-02 10:14:06')");
	//query_appinfo_fromdb();
	DEBUG_INFO("version:%s",g_baseconf.version);

	create_worker(ipaddr_query_from_property,0);
	//create_worker(heartbeat_to_property,0);
	create_worker(otherinner_ipaddr_query_from_property,0);

#ifdef ANDROID

/*
	for(i=0;i<g_seczone_count;i++) {
		char index[8];
		bzero(index,8);
		sprintf(index,"%d",i);
		printf("create thread %d\n",i);
*/
#ifdef YANGCHUANG
	for(i=0;i<g_seczone_count;i++) {
		SetIoMode(i,IO_INTR_MODE,IO_INTR_BOTH_EDGE_TRIGGERED);
	}
	create_worker(get_gpio_status,NULL);
#endif

#ifdef YANGCHUANG
        create_worker(watchdog_feeder,NULL);

#endif


#ifdef MAICHONG
	system("/system/xbin/echo_test &");
        create_worker(watchdog_feeder,NULL);
	
	create_worker(get_gpio_status,NULL);
#endif

        create_worker(app_watchdog,NULL);

#endif	

        create_worker(server_watchdog,NULL);
        create_worker(check_innerdb,NULL);

	jrpc_server_init(&my_in_server, IN_PORT);
	jrpc_server_init(&my_out_server, OUT_PORT);
	jrpc_server_init(&my_sec_server, 6789);

	jrpc_register_procedure(&my_in_server, init_setup, "init_setup", NULL );
	jrpc_register_procedure(&my_in_server, say_hello, "sayHello", NULL );
	jrpc_register_procedure(&my_in_server, callin_set, "callinset", NULL );
	jrpc_register_procedure(&my_in_server, app_line_is_busy, "line_is_busy", NULL );
	jrpc_register_procedure(&my_in_server, callout_to_property, "callout_to_property", NULL );
	jrpc_register_procedure(&my_in_server, callaccept_from_property, "callaccept_from_property", NULL );
	jrpc_register_procedure(&my_in_server, call_outerbox, "call_outerbox", NULL );
	jrpc_register_procedure(&my_in_server, call_elevator, "call_elevator", NULL );
	jrpc_register_procedure(&my_in_server, call_elevator_inter, "call_elevator_inter", NULL );
	jrpc_register_procedure(&my_in_server, call_other_innerbox, "call_other_innerbox", NULL );
	jrpc_register_procedure(&my_in_server, call_accept_from_otherinner, "call_accept_from_otherinner", NULL );
	jrpc_register_procedure(&my_in_server, seczone_pass_set, "seczone_pass_set", NULL );
	jrpc_register_procedure(&my_in_server, seczone_conf_set, "seczone_conf_set", NULL );
	jrpc_register_procedure(&my_in_server, seczone_conf_require, "seczone_conf_require", NULL );
	jrpc_register_procedure(&my_in_server, seczone_onekey_set, "seczone_onekey_set", NULL );
	jrpc_register_procedure(&my_in_server, seczone_onekey_reset, "seczone_onekey_reset", NULL );
	jrpc_register_procedure(&my_in_server, seczone_record_history, "seczone_record_history", NULL );
	jrpc_register_procedure(&my_in_server, destroy_emergency, "destroy_emergency", NULL );
	jrpc_register_procedure(&my_in_server, verify_emergency_password, "verify_emergency_password", NULL );
	jrpc_register_procedure(&my_in_server, join_family, "option_joinfamily", NULL );
	jrpc_register_procedure(&my_in_server, join_permit, "join_permit", NULL );
	jrpc_register_procedure(&my_in_server, door_open_processing, "door_open_processing", NULL );
	jrpc_register_procedure(&my_in_server, opendoor_permit, "opendoor_permit", NULL );
	jrpc_register_procedure(&my_in_server, opendoor_deny, "opendoor_deny", NULL );
	jrpc_register_procedure(&my_in_server, call_ending_from_app, "call_ending", NULL );
	//jrpc_register_procedure(&my_in_server, call_ending_of_timeout_from_app, "call_ending_of_timeout", NULL );
	jrpc_register_procedure(&my_in_server, call_ending_from_app, "call_ending_of_timeout", NULL );
	jrpc_register_procedure(&my_in_server, msghistory_request, "messagehistory_request", NULL );
	jrpc_register_procedure(&my_in_server, callhistory_request, "callhistory_request", NULL );
	jrpc_register_procedure(&my_in_server, app_update_request, "app_update_request", NULL );
	jrpc_register_procedure(&my_in_server, outerbox_netstat, "outerbox_netstat", NULL );
	jrpc_register_procedure(&my_in_server, exit_server, "exit", NULL );
	jrpc_register_procedure(&my_in_server, seczone_emergency, "seczone_emergency", NULL );
	jrpc_register_procedure(&my_in_server, show_version, "show_version", NULL );
	jrpc_register_procedure(&my_out_server, show_version, "show_version", NULL );
	jrpc_register_procedure(&my_out_server, open_door, "opendoor", NULL );
	jrpc_register_procedure(&my_out_server, call_from_property, "call_from_property", NULL );
	jrpc_register_procedure(&my_out_server, call_from_otherinner, "call_from_otherinner", NULL );
	jrpc_register_procedure(&my_out_server, call_ending_from_out, "call_ending", NULL );
	//jrpc_register_procedure(&my_out_server, call_ending_of_timeout_from_out, "call_ending_of_timeout", NULL );
	jrpc_register_procedure(&my_out_server, call_ending_from_out, "call_ending_of_timeout", NULL );
	jrpc_register_procedure(&my_out_server, call_ending_of_otherinner, "call_ending_of_otherinner", NULL );
	jrpc_register_procedure(&my_out_server, video_record, "video_record", NULL );
	jrpc_register_procedure(&my_out_server, msg_push, "msgpush", NULL );
	jrpc_register_procedure(&my_out_server, update_push, "update_push", NULL );
	jrpc_register_procedure(&my_out_server, softupdate_innerbox, "softupdate_innerbox", NULL );
	jrpc_register_procedure(&my_out_server, softupdate_query, "softupdate_query", NULL );
	jrpc_register_procedure(&my_out_server, ipconf_updated, "ipconf_updated", NULL );
	jrpc_register_procedure(&my_out_server, start_shell, "start_shell", NULL );
	jrpc_register_procedure(&my_sec_server, seczone_emergency, "seczone_emergency", NULL );
	jrpc_register_procedure(&my_sec_server, show_version, "show_version", NULL );
	
	//heartbeat_to_property();

	jrpc_server_run(&my_in_server);
//	jrpc_server_run(&my_out_server);
//	jrpc_server_run(&my_sec_server);

	jrpc_server_destroy(&my_in_server);
	jrpc_server_destroy(&my_out_server);
	jrpc_server_destroy(&my_sec_server);
	return 0;
}
