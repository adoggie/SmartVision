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
#include <sys/file.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <linux/watchdog.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>

#include "request.h"
#include "app_client.h"

#define VERSION "1.5.3.%s.%d-mc-I"

/**

revsion:

v1.4.0  2018/11/23 scott
  1. 清除 GET_LOCAL_ADDRESS()函数 的错误处理 ，置空，机器长时间并行连接正常，无资源泄露
  2. 实现分布式呼叫功能
  3. 修复 假死、连接挂不断等问题
  4. 修复 jsonrpc的解码错误代码，增加 yajl 支持 json的streaming处理

v1.4.1  2018/11/27 scott
  1. 增加 获取 主机 ip的函数  get_local_ip()

v1.4.3  2018/12/1 scott
  1. 将所有的sleep()调用改为 thread_sleep()
  2. 增加request.c
  3. comment "otherinner_ipaddr_query_from_property()" 线程
  sleep函数与 libev 有冲突(android) ,采用 select()等待模式，
  4. 本地ip从 /data/user/local.txt 读取

v1.4.5 2018/12/4 scott
  1. jsonrpc 修改 ev_default_loop(SELECT)

v1.4.6 2018/12/6 scott
  1. ipaddr_query_of_outerbox 查询室外机改用 室内机ip地址

v1.4.7 2018/12/9 scott
  1. 支持小区户外机呼叫进入

 v1.4.8 2018/12/11 scott
  1. 心跳heartbeat_to_property()取消ev方式
  2. 取消 ev 调用 new_connector(),直接用socket方式

 v1.4.9 2018/12/12 scott
 1. remove libev , use my original socket(select)

 v1.4.10 2018/12/13 scott
 1. remove 'ev_loop_new()' 调用，Fixed: fd leaks.

 v1.4.11 2018/12/14 scott
 1. fixed: send_recv_msg() 释放 cjson-root
 2. 对socket接收recv() 增加 select超时检测，防止文件句柄和线程由于对点未返回而挂起.
 3. 增加远程重启接口
 4. 增加远程查询主机运行状态的接口

 v1.5.0 2018/12/15 scott
 1. 修正 call_ending_of_otherinner() 未设置返回值导致内存释放异常奔溃(send_response)
 2. 关闭 app watchdog 线程
 3. 1.4.x 版本多app连接存在不稳定和异常，代码混乱不可维护， 故1.5.x去除多app连接管理，仅提供同主机的一个室内app连接，简化管理，稳定版本

v1.5.1 2018/12/16 scott
 1.去除join_family() 时对qr_code的校验，连接设备不入app_info表

v1.5.2 2018/12/18 scott
 1. 在app定时查询物业状态的函数(propertyserver_netstat)增加获取主机ip功能，解决由于主机启动时未插网线导致的ip地址无法获取的问题，发现ip未获取则重新获取
 2. 增加 2分钟检测呼叫状态功能，超过2分钟非IDLE则强制设置为IDLE 状态 call_status_check()
 3. 未接网线启动主机未获得ip的情况下，允许加入家庭，房号默认99-99-99，定时器检测网络恢复（重新获得ip）时触发app重新加入家庭。
 4. 修正 outerbox_netstat,propertyserver_netstat,send_msg 函数中setnio之后引发connect，send导致程序异常退出的问题。
  （android在无网卡状态下，connect未成功，之后调用send() 导致进程退出)

v1.5.2.2 2018/12/19 scott
 1. 启动时获取ip等待30秒

v1.5.3  2018/12/20 scott
 1. 脱机状态（未插网卡）propertyserver_netstat()定时检测到网卡恢复时，重启系统
 */

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
//#include "ipcheck.h"
#include "urlregex.h"

#define PORT 1234  // the port users will be connecting to

struct jrpc_server my_in_server;
struct jrpc_server my_out_server;
struct jrpc_server my_sec_server;
char *opencmd = "open the door please!\n";

#ifdef ANDROID/*all android file move to /data/user for more space 2018.06.09*/
#define PREFIX "/data/user"
#else
#define PREFIX "/etc"
#endif

#define INNERBOX_CONF PREFIX"/innerbox.conf"
#define INNERBOX_CONF_BAK PREFIX"/innerbox.conf.bk"
#define BASIC_CONF PREFIX"/basic.conf"
#define BASIC_CONF_BAK PREFIX"/basic.conf.bk"
#define SECZONE_CONF PREFIX"/seczone.conf"
#define SECZONE_CONF_BAK PREFIX"/seczone.conf.bk"
#define EXTEND_CONF PREFIX"/extend.conf"
#define EXTEND_CONF_BAK PREFIX"/extend.conf.bk"
#define INNER_DB PREFIX"/inner.db"
//#define CALLHISTORY_PATH "/var/www/html/callhisory/"
#define CALLHISTORY_PATH "/data/local/tmp/"
//#define MSGHISTORY_PATH "/var/www/html/msghisory/"

#define SECMSG_FILE PREFIX"/secmsg.db"
#define SECMSG_FILE_T PREFIX"/secmsg.db.tmp"

#ifdef ANDROID
#define MSGHISTORY_PATH "/data/local/tmp/"
#else
#define MSGHISTORY_PATH "/var/www/html/msghisory/"
#endif

#define ZONE_NUMBER 16

#define PAGE_NUMBER 10

//#define PID_PATH "/etc/serverpid"
#define PID_PATH PREFIX"/serverpid"

#define ACCESS_PERMISSION_DENIED "Permission Denied.Join Family First!"
#define WRONG_PARAMETER "Wrong Parameters!"
#define LINE_BUSY "line_is_busy"

basic_conf_t g_baseconf;
char g_localnetmask[64];
seczone_conf_t g_seczone_conf[ZONE_NUMBER]; //max 16
int g_seczone_count = 0;    //实际的防区数量
app_client_t *g_app_client_list = NULL;
char g_seczone_passwd[512];

int g_seczone_change = 0; //conf is change
pthread_mutex_t seczone_mutex;

// scott 2018.12.3
pthread_mutex_t g_conf_mutex = PTHREAD_MUTEX_INITIALIZER; // 全局参数保护
pthread_mutex_t g_innerboxip_mutex = PTHREAD_MUTEX_INITIALIZER; // 全局参数保护

void mutex_lock(pthread_mutex_t *mutex);

void mutex_unlock(pthread_mutex_t *mutex);

#define G_CONF_LOCK mutex_lock(&g_conf_mutex);
#define G_CONF_UNLOCK mutex_unlock(&g_conf_mutex);



char g_last_caller[64];
char g_call_from_innerbox[2048];

char g_callotherinner_doorid[32];
char g_otherinnerip[128];

int g_outerbox_online = 0;

int g_line_is_busy = 0;

int g_onekey_set = 0;

int g_seczone_mode = 0;

#define MODE_NUMBER 4
char g_seczone_mode_conf[MODE_NUMBER][64];

int g_last_nursetime = 0;

time_t g_secmsg_fail_time = 0;

int g_ipconf_get_flag = 0;
int g_innerbox_get_flag = 0;
int g_innerbox_count = 0;
innerbox_info_t *g_innerbox_info_list;

int g_update_state = 0;
int g_conf_stat = 0;
int g_watchdog_stop = 0; // scott 2018.12.14

int g_nocallin_delay = 0; //免打扰

char g_urlstring[1024];

#define SECMSG_MAX 100
char g_secmsg_to_send[SECMSG_MAX][512];
int g_secmsg_index = 0;

pthread_mutex_t sec_mutex;

#define UF 1
#define GF 2
#define BF 4
#define IF 8
#define OF 16
#define PF 32
#define SF 64
#define CF 128
#define AF 256
#define AAF 256*2
#define UVF 256*4
#define BVF 256*8
#define GVF 256*16


#define LOG 1

#ifdef SQL
#include <sqlite3.h>
#endif

/**connect out**/
typedef struct connect_watcher {
    ev_io eio;
    struct addrinfo *addr; /* addrinfo for this connection. */
    struct addrinfo *addr_base; /* The base of the addrinfo list. */
    int pos;
    unsigned int buffer_size;
    char *buffer;
    char *data;
} connect_watcher;

connect_watcher g_connectout_watcher;
//g_connectout_watcher;


void thread_sleep(seconds);

connect_watcher *new_connector(EV_P_ char *addrstr, char *portstr);

void create_basic_conf_file(void);

void create_extend_conf_file(void);

void query_info_from_property(char *cmdstr, struct ev_loop *loop);

int get_peer_address(int fd, char *peeraddress);

void init_stream_url();

void callhistory_log_to_db(char *time, char *person, char *event, char *peertype, char *peerid);

void get_time_now(char *datetime);

void get_local_address();

void *ipaddr_query_from_property();

int send_cmd_to_local(char *serveraddr, char *cmd);

static void create_worker(void *(*func)(), void *arg);

void seczonehistory_log_to_db(char *time, char *type, char *seczone, char *event);

void check_and_set_app_offline(app_client_t *appt);

void send_call_ending_to_connecting_app();

int eth_stat(char *ifname);

void g_onekey_set_func();

int send_recv_msg(const char *dest, unsigned short port, const char *msg, size_t size);


void travel_seczone_conf();

void backup_basic_conf();

void backup_extend_conf();

void reset_app_state();

void get_sys_version();

void get_basic_conf();

int g_ipgetted = 0;

int select_idle_app_and_send_cmd(call_type_e calltype, char *peeraddress, int peerport, int peersocketfd, void *param);

void get_propertybox_name_byipaddr(char *ipaddr, char *name) {
    int i = 0;
    if (strcmp(g_baseconf.callcenterip, ipaddr) == 0) {
        strncpy(name, "物业中心", 64);
        return;
    }

    for (i = 0; i < g_baseconf.sentryboxcount; i++) {
        if (strcmp(g_baseconf.sentrybox[i].ipaddr, ipaddr) == 0) {
            strncpy(name, g_baseconf.sentrybox[i].name, 64);
            break;
        }
    }
}

void generate_iptables_rules(char *serverip, int serverport, char *clientip, int clientport) {

    char rules[256];

    system("echo \"1\" > /proc/sys/net/ipv4/ip_forward");

    //generate iptables
    //iptables -t nat -A PREROUTING -i eth0 -d 172.18.44.44 -p tcp --dport 2321 -j DNAT --to 100.100.100.101:23
    //iptables -t nat -A PREROUTING -i eth0 -d 172.18.44.44 -p tcp --dport 8081 -j DNAT --to 100.100.100.101:80
    //# 第二台设备的telnet、web服务
    bzero(rules, 256);
    snprintf(rules, 256, "iptables -t nat -A PREROUTING -i %s -d %s -p tcp --dport %d -j DNAT --to %s:%d",
             g_baseconf.innerinterfacename, serverip, serverport, clientip, clientport);
    system(rules);
    bzero(rules, 256);
    snprintf(rules, 256, "iptables -t nat -A POSTOUTING -o %s -j MASQUERADE", g_baseconf.outerinterfacename);
    system(rules);
    snprintf(rules, 256, "iptables -A FORWARD -d %s -j ACCEPT", serverip);
    system(rules);

    //规则去重

    system("iptables-save > /tmp/iptables");
    system("cat /tmp/iptables |sed -n  'G; s/\n/&&/;/^\\(.*\n\\).*\n\1/d; s/\n//;h;P' >/tmp/iptables2");
    system("iptables-restore < /tmp/iptables2");
    system("rm -rf /tmp/iptables*");

}

void GET_LOCAL_ADDRESS() {
    return; // scott 20181123
    if (g_ipgetted == 1)
        return;
    if (eth_stat(g_baseconf.innerinterfacename) == 1) {
        get_local_address();
        create_basic_conf_file();
        if (strlen(g_baseconf.innerinterfaceip) > 0) {
            g_ipgetted = 1;
            reset_app_state();
        }
    } else {
        get_basic_conf();
        if (strlen(g_baseconf.innerinterfaceip) == 0)
            strcpy(g_baseconf.innerinterfaceip, "127.0.0.1");

        if (strlen(g_baseconf.outerinterfaceip) == 0)
            strcpy(g_baseconf.outerinterfaceip, "127.0.0.1");
    }

}

void backup_seczone_conf_file() {
    char cmd[256];
    bzero(cmd, 256);
    snprintf(cmd, 256, "cp %s %s", SECZONE_CONF, SECZONE_CONF_BAK);
    system(cmd);
    system("sync");
}

void recovery_seczone_conf_file() {
    char cmd[256];
    bzero(cmd, 256);
    snprintf(cmd, 256, "cp %s %s", SECZONE_CONF_BAK, SECZONE_CONF);
    system(cmd);
    system("sync");
}

void backup_innerbox_conf() {
    char cmd[256];
    bzero(cmd, 256);
    snprintf(cmd, 256, "cp %s %s", INNERBOX_CONF, INNERBOX_CONF_BAK);
    system(cmd);
    system("sync");
}

void get_rand_str(char s[], int number) {
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
    srand((unsigned int) time((time_t *) NULL));
    for (i = 1; i <= number; i++) {
        sprintf(ss, "%c", str[(rand() % 61) + 1]);
        strcat(s, ss);
    }

#endif
}

void generate_audio_stream(char *ipstr, char *outstream, char *randstr) {
    sprintf(outstream, g_baseconf.appaudiostreamurl, ipstr, randstr);
}

void
update_app_line_state(app_client_t *appt, call_type_e type, int port, char *ipaddr, int socket, app_state_e state) {
	printf("==update_app_line_state() ipaddr:%s \n",ipaddr);
    appt->line_state->calltype = type;
    appt->line_state->peerport = port;
    appt->line_state->peersocketfd = socket;
    appt->current_state = state;
    if (ipaddr != NULL)
        strcpy(appt->line_state->peeripaddr, ipaddr);
    else {
        bzero(appt->line_state->peeripaddr, sizeof(appt->line_state->peeripaddr));
    }


}

int get_local_sockinfo(int fd) {
    printf("== (get_local_sockinfo) current time: %d", time(NULL));

    struct sockaddr_storage localAddr;//连接的对端地址
    socklen_t localLen = sizeof(localAddr);
    int ret = 0;
    char ipstr[128];
    bzero(ipstr, 128);
    int port = 0;
    memset(&localAddr, 0, sizeof(localAddr));
    //get the app client addr
    ret = getsockname(fd, (struct sockaddr *) &localAddr, &localLen);
    if (ret == -1) perror("getsockname error!");
    if (localAddr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *) &localAddr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
    } else if (localAddr.ss_family == AF_INET6) {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *) &localAddr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
    }

    //strcpy(localaddress,ipstr);
    DEBUG_INFO("connected peer address = %s:%d", ipstr, port);

    printf("== (get_local_sockinfo) end time: %d", time(NULL));
    return port;

}


innerbox_info_t *malloc_innerbox_info() {
    innerbox_info_t *newinnerbox = malloc(sizeof(innerbox_info_t));
    bzero(newinnerbox, sizeof(innerbox_info_t));
    newinnerbox->next = NULL;

    return newinnerbox;
}

void delete_innerbox_info_list() {
    innerbox_info_t *loop = g_innerbox_info_list;
    innerbox_info_t *delit;
    while (loop) {
        delit = loop;
        loop = loop->next;
        free(delit);
    }
    g_innerbox_info_list = NULL;
}

innerbox_info_t *select_from_innerbox_info_list_byid(char *doorid) {
    innerbox_info_t *loop = g_innerbox_info_list;
    while (loop) {
        if (strcmp(doorid, loop->doorid) == 0)
            return loop;
        loop = loop->next;
    }
    return NULL;
}

innerbox_info_t *select_from_innerbox_info_list_byip(char *ipaddr) {
    innerbox_info_t *loop = g_innerbox_info_list;
    while (loop) {
        if (strcmp(ipaddr, loop->ipaddr) == 0)
            return loop;
        loop = loop->next;
    }
    return NULL;
}

void insert_into_innerbox_info_list(char *doorid, char *ipaddr) {
//	GET_LOCAL_ADDRESS();

    // scott 2018.12.3
//	if(strcmp(ipaddr,g_baseconf.outerinterfaceip) == 0)
//		strcpy(g_baseconf.doorid,doorid);

    innerbox_info_t *nbox = malloc_innerbox_info();
    strcpy(nbox->doorid, doorid);
    strcpy(nbox->ipaddr, ipaddr);
    innerbox_info_t *loop1 = g_innerbox_info_list;
    if (loop1 == NULL) {
        g_innerbox_info_list = nbox;
        return;
    }
    while (loop1) {
        if (loop1->next == NULL) {
            loop1->next = nbox;
            return;
        }
        loop1 = loop1->next;
    }
}

void create_innerbox_info_conf(void) {
    FILE *ini;
    innerbox_info_t *loop = g_innerbox_info_list;
    if (g_innerbox_count == 0)
        return;
    ini = fopen(INNERBOX_CONF, "w");
    fprintf(ini,
            "#\n"
            "# innerbox configure file\n"
            "#\n"
            "\n"
            "[doorcount]\n"
            "number = %d ;\n"
            "\n", g_innerbox_count);
    int i = 0;
    while (loop) {
        fprintf(ini, "[door%d]\n"
                     "doorid = %s ;\n"
                     "ipaddr = %s ;\n"
                     "\n", i, loop->doorid, loop->ipaddr);
        i++;
        loop = loop->next;
    }
    fflush(ini);
    fclose(ini);
//      fflush(ini);

}

void recovery_innerbox_conf() {
    char cmd[256];
    bzero(cmd, 256);
    snprintf(cmd, 256, "cp %s %s", INNERBOX_CONF_BAK, INNERBOX_CONF);
    system(cmd);
    system("sync");
}

int read_innerbox_info_from_conf(void) {
    dictionary *ini;

    /* Some temporary variables to hold query results */
    int b;
    int i, j;
    double d;
    char *s;
    char doorid[32];
    char ipaddr[64];
    char inistring[64];

    bzero(doorid, 32);
    bzero(ipaddr, 64);
    bzero(inistring, 64);

    if (g_innerbox_info_list)
        return 0;
    ini = iniparser_load(INNERBOX_CONF);
    if (ini == NULL) {
        DEBUG_ERR_LOG("cannot parse file: %s", INNERBOX_CONF);
        return -1;
    }

    /* Get attributes */
    GET_LOCAL_ADDRESS();

    DEBUG_INFO("innerbox count:");
    g_innerbox_count = iniparser_getint(ini, "doorcount:number", 1);
    DEBUG_INFO("count: [%d]", g_innerbox_count);
    GET_LOCAL_ADDRESS();
    for (j = 0; j < g_innerbox_count; j++) {
        bzero(doorid, 32);
        bzero(ipaddr, 64);
        bzero(inistring, 64);
        snprintf(inistring, 64, "door%d:doorid", j);
        s = iniparser_getstring(ini, inistring, NULL);
        if (s)
            strncpy(doorid, s, 32);
        bzero(inistring, 64);
        snprintf(inistring, 64, "door%d:ipaddr", j);
        s = iniparser_getstring(ini, inistring, NULL);
        if (s)
            strncpy(ipaddr, s, 64);
        insert_into_innerbox_info_list(doorid, ipaddr);
//                if( strcmp(ipaddr,"11.41.1.11") == 0){
//                    printf("==== local ip: %s, room:%s \n",ipaddr,doorid);
//                }
//      scott  阿福这个白痴，取了室外机ip当房号
//		if(strcmp(ipaddr,g_baseconf.outerinterfaceip) == 0)
//			strcpy(g_baseconf.doorid,doorid);
//        }

        if (strcmp(ipaddr, g_baseconf.innerinterfaceip) == 0)
            strcpy(g_baseconf.doorid, doorid);
    }

    iniparser_freedict(ini);

    return 0;
}

void travel_innerbox_infolist() {
    innerbox_info_t *loop = g_innerbox_info_list;

    while (loop) {
        DEBUG_INFO("doorid:%s,ipaddr:%s", loop->doorid, loop->ipaddr);
        loop = loop->next;
    }
}

int
set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1)
        return -1;
    return 0;
}

static void init_callotherinner_stream_url(char *streamfrom, char *fromdoorid, char *streamto, char *todoorid) {
    //sprintf(streamfrom,"rtmp://%s:1935/hls/callinner_%s",g_baseconf.streamserverip,fromdoorid);
    //sprintf(streamto,"rtmp://%s:1935/hls/callinner_%s",g_baseconf.streamserverip,todoorid);
    sprintf(streamfrom, g_baseconf.appaudiostreamurl, g_baseconf.streamserverip, fromdoorid);
    sprintf(streamto, g_baseconf.appaudiostreamurl, g_baseconf.streamserverip, todoorid);
}

//void * do_call_other_innerbox(char * otherinnerip)
void *do_call_other_innerbox(app_client_t *appt) {

    pthread_detach(pthread_self());

    //char cmdstr[2048];
    char portstr[16];
    bzero(portstr, 16);
    snprintf(portstr, 16, "%d", OUT_PORT);

    char streamfrom[512];
    bzero(streamfrom, 512);
    char streamto[512];
    bzero(streamto, 512);


//	struct ev_loop *loopinthread =ev_loop_new(0);
    printf("=== (do_call_other_innerbox) try to connection : %s,%s\n", appt->calling_otherinner_ip, portstr); // scott

//	connect_watcher *c = new_connector(loopinthread,appt->calling_otherinner_ip, portstr);
//	if(!c) {
//		DEBUG_ERR_LOG("Cannot connect to property server");
//		char tmpstr[512];
//		bzero(tmpstr,512);
//		snprintf(tmpstr,512,"{\"result\":\"call_error\",\"message\":\"无法连接到：%s\"}",appt->calling_otherinner_ip);
//		write(appt->socket_fd,tmpstr,sizeof(tmpstr));
//		return NULL;
//	}
//
//	//memset(&g_connectout_watcher,0,sizeof(struct connect_watcher));
//	c->buffer_size = 1500;
//	if(!c->buffer)
//		c->buffer = malloc(1500);
//	memset(c->buffer, 0, 1500);
//	c->pos = 0;

    //write command to property
// 	send(c->eio.fd,cmdstr,strlen(cmdstr),0);
    init_callotherinner_stream_url(streamfrom, g_baseconf.doorid, streamto, appt->calling_otherdoorid);
    char callotherinner_cmdstr[1024];
    bzero(callotherinner_cmdstr, 1024);
    snprintf(callotherinner_cmdstr, 1024,
             "{\"method\":\"call_from_otherinner\",\"params\":{\"doorid\":\"%s\",\"stream_url_from_otherinner\":\"%s\",\"stream_url_to_otherinner\":\"%s\"}}",
             g_baseconf.doorid, streamfrom, streamto);
//	c->data = callotherinner_cmdstr;
//	c->eio.data = c;

//	if (c) {
    if (1) {
        printf("Trying connection to %s:%d...", appt->calling_otherinner_ip, OUT_PORT);
//		update_app_line_state(appt,APP2APP,OUT_PORT,appt->calling_otherinner_ip,c->eio.fd,CONNECTING);
        update_app_line_state(appt, APP2APP, OUT_PORT, appt->calling_otherinner_ip, 0, CONNECTING);
        char timestr[64];
        bzero(timestr, 64);

        get_time_now(timestr);
        callhistory_log_to_db(timestr, appt->calling_otherdoorid, "呼出", "住户", appt->calling_otherdoorid);

        send_recv_msg(appt->calling_otherinner_ip, OUT_PORT, callotherinner_cmdstr, strlen(callotherinner_cmdstr));

//		ev_io_start(loopinthread, &c->eio);
//		ev_run(loopinthread, 0);
    }
//	else {
//	       	DEBUG_ERR_LOG( "Can't create a new connection ");
//		return NULL;
//	}

}

void send_callaccept_from_otherinner_to_app(char *raddress, int rport) {
    char resultstr[1024];
    bzero(resultstr, 1024);
    char streamfrom[512];
    bzero(streamfrom, 512);
    char streamto[512];
    bzero(streamto, 512);
    int ret = 0;

    init_callotherinner_stream_url(streamfrom, g_baseconf.doorid, streamto, g_callotherinner_doorid);
    snprintf(resultstr, 1024,
             "{\"result\":\"call_accept_from_otherinner\",\"params\":{\"stream_url_from_otherinner\":\"%s\",\"stream_url_to_otherinner\":\"%s\"}}^",
             streamto, streamfrom);

    printf("==remote accept call, send to app cmd: %s \n", resultstr);

//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
//    while (appt) {
    if(1){
        check_and_set_app_offline(appt);

        if (appt->current_state == CONNECTING && appt->line_state->calltype == APP2APP &&
            strcmp(appt->line_state->peeripaddr, raddress) == 0 && appt->line_state->peerport == rport) {
            appt->current_state = CALLING;
//            ret = write(appt->socket_fd, resultstr, strlen(resultstr));
            ret = send_app_data(resultstr, strlen(resultstr));
            printf("==send to App ok..\n");
        }

        if (ret == -1) {
            update_app_line_state(appt, ZERO, 0, NULL, 0, OFFLINE);
        }
//        appt = appt->next;
    }

}

void save_ipconf_of_innerbox(cJSON *root) {
    cJSON *ipaddrarray = cJSON_GetObjectItem(root, "values");
    cJSON *item, *doorid, *ipaddr;
    GET_LOCAL_ADDRESS();
    if (ipaddrarray != NULL && ipaddrarray->type == cJSON_Array) {
        g_innerbox_count = cJSON_GetArraySize(ipaddrarray);
        g_innerbox_get_flag = g_innerbox_count;
        int i = 0;
        delete_innerbox_info_list();
        while (i < g_innerbox_count) {
            item = cJSON_GetArrayItem(ipaddrarray, i);
            if (item != NULL && item->type == cJSON_Object) {
                doorid = cJSON_GetObjectItem(item, "doorid");
                ipaddr = cJSON_GetObjectItem(item, "ipaddr");
                if ((doorid != NULL && doorid->type == cJSON_String) &&
                    (ipaddr != NULL && ipaddr->type == cJSON_String))
                    insert_into_innerbox_info_list(doorid->valuestring, ipaddr->valuestring);
            }
            i++;
        }
        create_innerbox_info_conf();
        backup_innerbox_conf();
        reset_app_state();
        //travel_innerbox_infolist();
    }

}

void save_ipconf_of_outerbox(cJSON *root) {
//    return ;

    G_CONF_LOCK;


    cJSON *ipaddr = cJSON_GetObjectItem(root, "outerbox_f1");

    if (ipaddr != NULL && ipaddr->type == cJSON_String) {
        strcpy(g_baseconf.outerboxip, ipaddr->valuestring);
    }
    ipaddr = cJSON_GetObjectItem(root, "gateouterbox");

    if (ipaddr != NULL && ipaddr->type == cJSON_String) {
        strcpy(g_baseconf.gateouterboxip, ipaddr->valuestring);
    }
    ipaddr = cJSON_GetObjectItem(root, "outerbox_x1");

    if (ipaddr != NULL && ipaddr->type == cJSON_String) {
        strcpy(g_baseconf.outerboxip_1, ipaddr->valuestring);
    }

/*
	ipaddr = cJSON_GetObjectItem(root,"streamserver");

	if(ipaddr != NULL && ipaddr->type == cJSON_String) {
		strcpy(g_baseconf.streamserverip,ipaddr->valuestring);
	}
*/

    // scott 2018.12.3  注释以下三行，设备不卡顿，具体原因待查
    g_ipconf_get_flag = 1;
    create_extend_conf_file();
    backup_extend_conf();

    G_CONF_UNLOCK;

}

void send_linebusy_to_app(char *raddress, int rport) {
    char *busystr = "{\"result\":\"line_is_busy\"}^";
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
//    while (appt) {
    if(1){
        check_and_set_app_offline(appt);
        if (appt->current_state == CONNECTING &&
            (appt->line_state->calltype == APP2PROPERTY || appt->line_state->calltype == APP2APP) &&
            strcmp(appt->line_state->peeripaddr, raddress) == 0 && appt->line_state->peerport == rport) {
//            int ret = write(appt->socket_fd, busystr, strlen(busystr));
            int ret = send_app_data( busystr, strlen(busystr));
            if (ret > 0)
                update_app_line_state(appt, ZERO, 0, NULL, 0, IDLE);
            else
                update_app_line_state(appt, ZERO, 0, NULL, 0, OFFLINE);
//            break;

        }

//        appt = appt->next;
    }

}

void send_call_accept_to_app(cJSON *root, char *raddress, int rport) {
    cJSON *audio = cJSON_GetObjectItem(root, "audio");
    if ((audio != NULL) && (audio->type == cJSON_String)) {

//        app_client_t *appt = g_app_client_list;
        app_client_t *appt = get_app_client();
//        while (appt) {
        if(1){
            check_and_set_app_offline(appt);
            if (appt->line_state->calltype == APP2PROPERTY && appt->current_state == CONNECTING &&
                strcmp(appt->line_state->peeripaddr, raddress) == 0 && appt->line_state->peerport == rport) {

                cJSON *result = cJSON_CreateObject();
                cJSON *content = cJSON_CreateObject();
#ifdef ALL_IN_ONE
                cJSON_AddStringToObject(content,"audio_from_property",audio->valuestring);
#else
//                char rtmpstr[1024];
//                bzero(rtmpstr, 1024);
//                char sip[64];
//                bzero(sip, 64);
//
//                GET_LOCAL_ADDRESS();
//
//                url_replace(audio->valuestring, rtmpstr, g_baseconf.innerinterfaceip, 1936);
//                extract_ip_by_regex(audio->valuestring, sip);
//                int sport = extract_port_by_regex(audio->valuestring);
//                generate_iptables_rules(sip, sport, g_baseconf.innerinterfaceip, 1936);
//                //snprintf(rtmpstr,1024,"rtmp://%s:1936/hls/stream",g_baseconf.innerinterfaceip);
//                cJSON_AddStringToObject(content, "audio_from_property", rtmpstr);
#endif
                cJSON_AddStringToObject(content, "audio_to_property", appt->audiostream_url);
                cJSON_AddItemToObject(result, "callout_params", content);
                cJSON *sendout = cJSON_CreateObject();
                cJSON_AddItemToObject(sendout, "result", result);
                char *str_send = cJSON_Print(sendout);
                char str_send0[1024];
                bzero(str_send0, 1024);
                snprintf(str_send0, 1024, "%s^", str_send);
//                int ret = write(appt->socket_fd, str_send0, strlen(str_send0));
                int ret = send_app_data( str_send0, strlen(str_send0));
                free(str_send);
                cJSON_Delete(sendout);

                appt->current_state = CALLING;
                if (ret == -1)
                    appt->current_state = OFFLINE;

                char timestr[128];
                bzero(timestr, 128);
                char event[128];
                bzero(event, 128);
                get_time_now(timestr);
                strcpy(event, "对方接听");
                //callhistory_log_to_db(timestr,g_baseconf.propertyip,event);
                char name[64];
                bzero(name, 64);
                get_propertybox_name_byipaddr(appt->line_state->peeripaddr, name);
                callhistory_log_to_db(timestr, appt->line_state->peeripaddr, event, name, appt->line_state->peeripaddr);

//                break;
            }
//            appt = appt->next;
        }
    }
}

void send_qrcode_opendoor_authcode_to_app(char *qrcodeinfo) {
    char cmdstr[1024] = {0};
    if (qrcodeinfo == NULL)
        return;

    snprintf(cmdstr, 256, "{\"result\":\"qrcode_opendoor_authcode\",\"value\":\"%s\"}^", qrcodeinfo);

//    app_client_t *appt = g_app_client_list;
//    while (appt) {
//        if (appt->app_state & GETTING_QRCODE == GETTING_QRCODE) {
//            write(appt->socket_fd, cmdstr, strlen(cmdstr));
//            appt->app_state &= ~GETTING_QRCODE;
//        }
//
//        appt = appt->next;
//    }

    send_app_data(cmdstr,strlen(cmdstr));

}



/**connect out end**/

void get_time_now(char *datetime) {
    time_t now;
    struct tm *tm_now;
    // char    datetime[200];

    time(&now);
    tm_now = localtime(&now);
    strftime(datetime, 200, "%Y-%m-%d %H:%M:%S", tm_now);

    DEBUG_INFO("now datetime: %s", datetime);

}

app_client_t *get_appt_by_ctx(jrpc_context *ctx) {
//    int fd = ((struct jrpc_connection *) (ctx->data))->fd;
//    app_client_t *appt = g_app_client_list;
//    while (appt != NULL) {
//        if (appt->socket_fd == fd)
//            return appt;
//        appt = appt->next;
//    }
//    return NULL;

    return get_app_client()->current_state >=JOINED ? get_app_client(): NULL;
}


/**
 *
 * @param ctx
 * @return  1 allow , 0 denied
 */
int access_check(jrpc_context *ctx) {
//    int fd = ((struct jrpc_connection *) (ctx->data))->fd;
//    app_client_t *appt = g_app_client_list;
//    while (appt != NULL) {
//        if (appt->socket_fd == fd)
//            break;
//        appt = appt->next;
//    }
//
//    if (appt && appt->current_state >= JOINED)
//        return 1;
//    else
//        return 0;

    return get_app_client()->current_state >= JOINED? 1:0;
}

#ifdef SQL
                                                                                                                        sqlite3 *g_memdb;
sqlite3 *g_filedb;

void db_insert_test()
{
	char timestr[64] = {0};
	int i = 0;
	time_t now;
        struct tm *tm_now;
	for(i = 0;i<100000;i++) {
		time(&now);
		now +=100000;
        	tm_now = localtime(&now);
        	strftime(timestr, 200, "%Y-%m-%d %H:%M:%S", tm_now);

		seczonehistory_log_to_db(timestr,"报警","#重发","成功");
	}
}

int attach_db()
{
	int      rc           =  0;
	char*    errMsg       =  NULL;
	char     sqlcmd[512]  =  {0};

	snprintf(sqlcmd, sizeof(sqlcmd), "ATTACH '%s' AS %s", INNER_DB, "filedb");
	rc = sqlite3_exec(g_memdb, sqlcmd, NULL, NULL, &errMsg);
	if (SQLITE_OK != rc) {
        	fprintf(stderr, "can't attach database %s:%s\n", INNER_DB, errMsg);
       		sqlite3_close(g_memdb);
        	return 0;
    	}

    	return 1;
}

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
	const char *sSQL1 = "CREATE TABLE IF NOT EXISTS appinfo(appid varchar(256) PRIMARY KEY, name varchar(64), type varchar(16), joindate datetime);";
	const char *sSQL2 = "CREATE TABLE IF NOT EXISTS updateinfo( devtype varchar(32) PRIMARY KEY,version varchar(32), updatetime datetime,filename varchar(1024));";
	const char *sSQL3 = "CREATE TABLE IF NOT EXISTS callhistory( calltime datetime PRIMARY KEY,person varchar(32),event varchar(512),peertype varchar(32),peerid varchar(32));";
	const char *sSQL4 = "CREATE TABLE IF NOT EXISTS msghistory( time datetime PRIMARY KEY,url varchar(256));";
	const char *sSQL5 = "CREATE TABLE IF NOT EXISTS seczonehistory( time datetime PRIMARY KEY,type varchar(32),seczone varchar(64),event varchar(512));";
	const char *sSQL6 = "ALTER TABLE callhistory ADD COLUMN peertype varchar(64);";
	const char *sSQL7 = "ALTER TABLE callhistory ADD COLUMN peerid varchar(32);";
//	const char *sSQL2 = "insert into users values('wang', 20, '1989-5-4');";
//	const char *sSQL3 = "select * from users;";

	sqlite3 *db = 0;
	char *pErrMsg = 0;
	int ret = 0;
	//连接数据库
	sqlite3_open(":memory:", &g_memdb);
	sqlite3_exec(g_memdb, sSQL1, 0, 0, &pErrMsg);
	sqlite3_exec(g_memdb, sSQL2, 0, 0, &pErrMsg);
	sqlite3_exec(g_memdb, sSQL3, 0, 0, &pErrMsg);
	sqlite3_exec(g_memdb, sSQL4, 0, 0, &pErrMsg);
	sqlite3_exec(g_memdb, sSQL5, 0, 0, &pErrMsg);
	sqlite3_exec(g_memdb, sSQL6, 0, 0, &pErrMsg);
	sqlite3_exec(g_memdb, sSQL7, 0, 0, &pErrMsg);

	ret = sqlite3_open(INNER_DB, &g_filedb);
	if (ret != SQLITE_OK)
	{
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));
	        sqlite3_close(db);
	        return -1;
	}
        DEBUG_INFO("数据库连接成功");

	db = g_filedb;
	//执行建表SQL
	ret = sqlite3_exec(db, sSQL1, 0, 0, &pErrMsg);
	if (ret != SQLITE_OK)
	{
	        fprintf(stderr, "SQL create error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！
	//        sqlite3_qllose(db);
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
	}

	ret = sqlite3_exec(db, sSQL6, 0, 0, &pErrMsg);
	if (ret != SQLITE_OK)
	{
	        fprintf(stderr, "SQL create error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！
	}

	ret = sqlite3_exec(db, sSQL7, 0, 0, &pErrMsg);
	if (ret != SQLITE_OK)
	{
	        fprintf(stderr, "SQL create error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！
	        //sqlite3_close(db);
	        return -1;
	}

        DEBUG_INFO("数据库建表成功！！");
	//sqlite3_close(db);
	return 0;
}

int insert_data_to_db(const char *sqlstr)
{
	sqlite3 *db = 0;
	char *pErrMsg = 0;
	int ret = 0;

	//连接数据库
	/*ret = sqlite3_open(INNER_DB, &db);
	if (ret != SQLITE_OK)
	{
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));
	        sqlite3_close(db);
	        return -1;
	}
        DEBUG_INFO("数据库连接成功");
	*/

	db = g_filedb;

	//执行查询操作
	ret = sqlite3_exec(db, sqlstr, 0, 0, &pErrMsg);
        if (ret != SQLITE_OK)
        {
		fprintf(stderr, "SQL insert error: %s\n", pErrMsg);
        	sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！
        	//sqlite3_close(db);
        	return -1;
    	}
     	DEBUG_INFO("数据库插入数据成功！");
	//sqlite3_close(db);
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
	/*
	ret = sqlite3_open(INNER_DB, &db);
	if (ret != SQLITE_OK)
	{
	        fprintf(stderr, "无法打开数据库：%s\n", sqlite3_errmsg(db));
	        sqlite3_close(db);
	        return -1;
	}
	*/
	db = g_filedb;
	//执行查询操作
	ret = sqlite3_exec(db, sqlstr, appinfo_sql_callback, flag, &pErrMsg);
    	if (ret != SQLITE_OK)
        {
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg);
	        //sqlite3_close(db);
	        return -1;
    	}
        DEBUG_INFO("数据库查询成功！！");
	//关闭数据库
	//sqlite3_close(db);
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

	GET_LOCAL_ADDRESS();

        cJSON *result_root = cJSON_CreateObject();
        cJSON *string = cJSON_CreateString(argv[1]);
        cJSON_AddItemToObject(result_root,"version",string);
	snprintf(urlstring,512,"http://%s/update/%s",g_baseconf.innerinterfaceip,argv[3]);
	string = cJSON_CreateString(urlstring);
        cJSON_AddItemToObject(result_root,"version",string);
	string = cJSON_CreateString(argv[2]);
        cJSON_AddItemToObject(result_root,"updatetime",string);

	cJSON_AddItemToObject(app,"app_update_info",result_root);
/*
	snprintf(returnstring,1024,"{\"app_update_info\":{\"version\":\"%s\",\"url\":\"http://%s/update/%s\",\"updatetime\":\"%s\"}}",argv[1],g_baseconf.innerinterfaceip,argv
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

	db = g_filedb;
	//执行查询操作
        cJSON *app = cJSON_CreateObject();
	ret = sqlite3_exec(db, sqlstr, updateinfo_sql_callback, app, &pErrMsg);
    	if (ret != SQLITE_OK)
        {
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg);
	        //sqlite3_close(db);
		cJSON_Delete(app);
	        return NULL;
    	}
        DEBUG_INFO("数据库查询成功！！");
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

	GET_LOCAL_ADDRESS();

	cJSON *content_obj = cJSON_CreateObject();
	cJSON *string = cJSON_CreateString(argv[0]);
	cJSON_AddItemToObject(content_obj,"calltime",string);
	snprintf(person,512,"http://%s/callhistory/%s",g_baseconf.innerinterfaceip,argv[1]);
	string = cJSON_CreateString(person);
	cJSON_AddItemToObject(content_obj,"person",string);
	string = cJSON_CreateString(argv[2]);
	cJSON_AddItemToObject(content_obj,"event",string);
	string = cJSON_CreateString(argv[3]);
	cJSON_AddItemToObject(content_obj,"peertype",string);
	string = cJSON_CreateString(argv[4]);
	cJSON_AddItemToObject(content_obj,"peerid",string);
	cJSON_AddItemToArray(content_array,content_obj);

	return 0;
}

cJSON * query_callhistory_fromdb(char *sqlstr,int page,int total)
{
	sqlite3 *db = 0;
	char *pErrMsg = 0;
	int ret = 0;
	db = g_filedb;

	//执行查询操作
        cJSON * content_array= cJSON_CreateArray();
	ret = sqlite3_exec(db, sqlstr, callhistory_sql_callback, content_array, &pErrMsg);
    	if (ret != SQLITE_OK)
        {
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg);
		cJSON_Delete(content_array);
	        return NULL;
    	}

	cJSON * callhistory = cJSON_CreateObject();
	cJSON_AddNumberToObject(callhistory,"page",page);
	cJSON_AddNumberToObject(callhistory,"total",total);
	cJSON_AddItemToObject(callhistory,"callhistory",content_array);

        DEBUG_INFO("数据库查询成功！！");
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
	db = g_filedb;
	//执行查询操作
	ret = sqlite3_exec(db, sqlstr, callhistory_count_callback, count, &pErrMsg);
    	if (ret != SQLITE_OK)
        {
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg);
	        //sqlite3_close(db);
	        return 0;
    	}

        DEBUG_INFO("数据库查询成功！！");

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
	//snprintf(url,512,"rtsp://%s:554/msghistory/%s",g_baseconf.innerinterfaceip,argv[1]);
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
	db = g_filedb;

	//执行查询操作
	ret = sqlite3_exec(db, sqlstr, msghistory_count_callback, count, &pErrMsg);
    	if (ret != SQLITE_OK)
        {
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg);
	        //sqlite3_close(db);
	        return 0;
    	}

        DEBUG_INFO("数据库查询成功！！");

	return atoi(count);
}

cJSON * query_msghistory_fromdb(char * sqlstr,int page,int total)
{
	sqlite3 *db = 0;
	char *pErrMsg = 0;
	int ret = 0;

	//连接数据库
	db = g_filedb;
	//执行查询操作
        cJSON * content_array= cJSON_CreateArray();
	ret = sqlite3_exec(db, sqlstr, msghistory_sql_callback, content_array, &pErrMsg);
    	if (ret != SQLITE_OK)
        {
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg);
		cJSON_Delete(content_array);
	        return NULL;
    	}

	cJSON * msghistory = cJSON_CreateObject();
	cJSON_AddNumberToObject(msghistory,"page",page);
	cJSON_AddNumberToObject(msghistory,"total",total);
	cJSON_AddItemToObject(msghistory,"messagehistory",content_array);

        DEBUG_INFO("数据库查询成功！！");
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

	db = g_filedb;
	//执行查询操作
	ret = sqlite3_exec(db, sqlstr, seczonehistory_count_callback, count, &pErrMsg);
    	if (ret != SQLITE_OK)
        {
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg);
	        return 0;
    	}

        DEBUG_INFO("数据库查询成功！！");

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

	db = g_filedb;

	//执行查询操作
        cJSON * content_array= cJSON_CreateArray();
	ret = sqlite3_exec(db, sqlstr, seczonehistory_sql_callback, content_array, &pErrMsg);
    	if (ret != SQLITE_OK)
        {
	        fprintf(stderr, "SQL error: %s\n", pErrMsg);
	        sqlite3_free(pErrMsg);
		cJSON_Delete(content_array);
	        return NULL;
    	}

	cJSON * seczonehistory = cJSON_CreateObject();
	cJSON_AddNumberToObject(seczonehistory,"page",page);
	cJSON_AddNumberToObject(seczonehistory,"total",total);
	cJSON_AddItemToObject(seczonehistory,"seczone_history",content_array);

        DEBUG_INFO("数据库查询成功！！");

	return seczonehistory;
}
#endif

void create_extend_conf_file(void) {
    FILE *ini;
    int i = 0;

    ini = fopen(EXTEND_CONF, "w");
    fprintf(ini,
            "#\n"
            "# extend configure file\n"
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
            "\n",
            g_baseconf.gateouterboxip,
            g_baseconf.outerboxip,
            g_baseconf.outerboxip_1
    );

    fflush(ini);
    fclose(ini);
}

int get_extendconf_fromini(char *ini_name) {
    dictionary *ini;

    /* Some temporary variables to hold query results */
    int b;
    int i;
    double d;
    char *s;
    char tmpstr[128];
    int j;

    ini = iniparser_load(ini_name);
    if (ini == NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return -1;
    }
    //iniparser_dump(ini, stderr);

    /* Get attributes */
    G_CONF_LOCK;

    DEBUG_INFO("outerboxip:");
    s = iniparser_getstring(ini, "outerbox:ip", NULL);
    DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    if (s) {
        strcpy(g_baseconf.outerboxip, s);
        strcpy(g_baseconf.outerinterfaceip, s); // scott

    }

    DEBUG_INFO("gateouterboxip:");
    s = iniparser_getstring(ini, "gateouterbox:ip", NULL);
    DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.gateouterboxip, s);

    DEBUG_INFO("outerboxip_1:");
    s = iniparser_getstring(ini, "outerbox_1:ip", NULL);
    DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.outerboxip_1, s);

    iniparser_freedict(ini);

    G_CONF_UNLOCK;

    return 0;
}

void create_basic_conf_file(void) {
    FILE *ini;
    int i = 0;

    ini = fopen(BASIC_CONF, "w");
    fprintf(ini,
            "#\n"
            "# basic configure file\n"
            "#\n"
            "\n"
            "#本机地址\n"
            "#\n"
            "[inneripconf]\n"
            "\n"
            "ip = %s ;\n"
            "mask = %s ;\n"
            "name = %s ;\n"
            "\n"
            "\n"
            "[outeripconf]\n"
            "\n"
            "ip = %s ;\n"
            "mask = %s ;\n"
            "name = %s ;\n"
            "\n"
            "\n"
            "#物业中心服务器\n"
            "#\n"
            "[propertyserver]\n"
            "\n"
            "ip = %s ;\n"
            "\n"
            "\n"
            "#推流服务器\n"
            "#\n"
            "[streamserver]\n"
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
            "#音频推流地址\n"
            "#\n"
            "[appaudiostream]\n"
            "\n"
            "url = %s ;\n"
            "\n\n"
            "#室外机视屏流地址1\n"
            "#\n"
            "[outerboxvideostream]\n"
            "\n"
            "url = %s ;\n"
            "\n\n"
            "#负一楼室外机视屏流地址\n"
            "#\n"
            "[outerboxvideostream_1]\n"
            "\n"
            "url = %s ;\n"
            "\n\n"
            "#门口室外机视屏流地址\n"
            "#\n"
            "[gateouterboxvideostream]\n"
            "\n"
            "url = %s ;\n"
            "\n\n"
            "#物业岗亭机\n"
            "#\n"
            "[sentrybox]\n"
            "\n"
            "count = %d ;\n"
            "\n"
            "\n",
            g_baseconf.innerinterfaceip,
            g_baseconf.innerinterfacemask,
            g_baseconf.innerinterfacename,
            g_baseconf.outerinterfaceip,
            g_baseconf.outerinterfacemask,
            g_baseconf.outerinterfacename,
            g_baseconf.propertyip,
            g_baseconf.streamserverip,
            g_baseconf.callcenterip,
            g_baseconf.alarmcenterip,
            g_baseconf.appaudiostreamurl,
            g_baseconf.outerboxvideostreamurl,
            g_baseconf.outerboxvideostreamurl_1,
            g_baseconf.gateouterboxvideostreamurl,
            g_baseconf.sentryboxcount
    );

    for (i = 0; i < g_baseconf.sentryboxcount; i++) {
        fprintf(ini,
                "[sentrybox%d]\n"
                "name= %s ;\n"
                "ipaddr= %s ;\n\n",
                i, g_baseconf.sentrybox[i].name, g_baseconf.sentrybox[i].ipaddr);
    }
    fflush(ini);
    fclose(ini);
}

int get_version_fromini(char *ini_name) {

    dictionary *ini;
    char *s;
    ini = iniparser_load(ini_name);
    if (ini == NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return -1;
    }

    DEBUG_INFO("version:");
    s = iniparser_getstring(ini, "version:ver", NULL);
    DEBUG_INFO("VER: [%s]\n", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.version, s);

    iniparser_freedict(ini);
    return 0;
}

int get_baseconf_fromini(char *ini_name) {
    dictionary *ini;

    /* Some temporary variables to hold query results */
    int b;
    int i;
    double d;
    char *s;
    char tmpstr[128];
    int j;

    ini = iniparser_load(ini_name);
    if (ini == NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return -1;
    }
    //iniparser_dump(ini, stderr);

    G_CONF_LOCK;

    /* Get attributes */

    DEBUG_INFO("localconf:");
    s = iniparser_getstring(ini, "inneripconf:ip", NULL);
    DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.innerinterfaceip, s);
    s = iniparser_getstring(ini, "inneripconf:mask", NULL);
    DEBUG_INFO("MASK: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.innerinterfacemask, s);
    s = iniparser_getstring(ini, "inneripconf:name", NULL);
    DEBUG_INFO("NAME: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.innerinterfacename, s);

    s = iniparser_getstring(ini, "outeripconf:ip", NULL);
    DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.outerinterfaceip, s);
    s = iniparser_getstring(ini, "outeripconf:mask", NULL);
    DEBUG_INFO("MASK: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.outerinterfacemask, s);
    s = iniparser_getstring(ini, "outeripconf:name", NULL);
    DEBUG_INFO("NAME: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.outerinterfacename, s);

    DEBUG_INFO("property:");
    s = iniparser_getstring(ini, "propertyserver:ip", NULL);
    DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.propertyip, s);
    else {
        iniparser_freedict(ini);

        G_CONF_UNLOCK;
        return -2;
    }



    DEBUG_INFO("stream:");
    s = iniparser_getstring(ini, "streamserver:ip", NULL);
    DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.streamserverip, s);

    DEBUG_INFO("callcenter:");
    s = iniparser_getstring(ini, "callcenter:ip", NULL);
    DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.callcenterip, s);

    DEBUG_INFO("alarmcenter:");
    s = iniparser_getstring(ini, "alarmcenter:ip", NULL);
    DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
    if (s)
        strcpy(g_baseconf.alarmcenterip, s);
    //sentry box info
    DEBUG_INFO("sentrybox:");
    i = iniparser_getint(ini, "sentrybox:count", 0);
    DEBUG_INFO("count:[%d]", i);
    g_baseconf.sentryboxcount = i;

    for (j = 0; j < i; j++) {
        bzero(tmpstr, 128);
        sprintf(tmpstr, "sentrybox%d:name", j);
        s = iniparser_getstring(ini, tmpstr, NULL);
        DEBUG_INFO("sentrybox%d:%s", j, s ? s : "UNDEF");
        if (s)
            strncpy(g_baseconf.sentrybox[j].name, s, 64);
        bzero(tmpstr, 128);
        sprintf(tmpstr, "sentrybox%d:ipaddr", j);
        s = iniparser_getstring(ini, tmpstr, NULL);
        DEBUG_INFO("sentrybox%d:%s", j, s ? s : "UNDEF");
        if (s)
            strncpy(g_baseconf.sentrybox[j].ipaddr, s, 64);

    }

    DEBUG_INFO("appaudiostream:");
    s = iniparser_getstring(ini, "appaudiostream:url", NULL);
    DEBUG_INFO("URL: [%s]", s ? s : "UNDEF");
    //scott 2018.12.3
    if (s) {
        strcpy(g_baseconf.appaudiostreamurl, s);
    } else {
        strcpy(g_baseconf.appaudiostreamurl, "rtmp://%s:1935/hls/%s");
    }


    DEBUG_INFO("outerboxvideostream:");
    s = iniparser_getstring(ini, "outerboxvideostream:url", NULL);
    DEBUG_INFO("URL: [%s]", s ? s : "UNDEF");

    //scott 2018.12.3
    if (s) {
        strcpy(g_baseconf.outerboxvideostreamurl, s);
    } else {
        strcpy(g_baseconf.outerboxvideostreamurl, "rtsp://admin:admin@%s:554/stream1");
    }

    DEBUG_INFO("outerboxvideostream_1:");
    s = iniparser_getstring(ini, "outerboxvideostream_1:url", NULL);
    DEBUG_INFO("URL: [%s]", s ? s : "UNDEF");

    //scott 2018.12.3
    if (s) {
        strcpy(g_baseconf.outerboxvideostreamurl_1, s);
    } else {
        strcpy(g_baseconf.outerboxvideostreamurl_1, "rtsp://admin:admin@%s:554/stream1");
    }

    DEBUG_INFO("gateouterboxvideostream:");
    s = iniparser_getstring(ini, "gateouterboxvideostream:url", NULL);
    DEBUG_INFO("URL: [%s]", s ? s : "UNDEF");

    //scott 2018.12.3
    if (s) {
        strcpy(g_baseconf.gateouterboxvideostreamurl, s);
    } else {
        strcpy(g_baseconf.gateouterboxvideostreamurl, "rtsp://admin:admin@%s:554/stream1");
    }

    iniparser_freedict(ini);

    if ((strlen(g_baseconf.callcenterip) == 0) && (strlen(g_baseconf.propertyip) > 0))
        strcpy(g_baseconf.callcenterip, g_baseconf.propertyip);
    if ((strlen(g_baseconf.alarmcenterip) == 0) && (strlen(g_baseconf.propertyip) > 0))
        strcpy(g_baseconf.alarmcenterip, g_baseconf.propertyip);

    G_CONF_UNLOCK;

    return 0;
}

//conig the address of the outer interface
void do_set_ip() {
    char cmd[256];
    bzero(cmd, 256);
#ifdef ANDROID
                                                                                                                            //if(netmask_and_ip_is_valid(g_baseconf.outerinterfaceip,g_baseconf.outerinterfacemask) == FALSE)
	//	return;

//	snprintf(cmd,256,"busybox ifconfig eth0 %s netmask %s",g_baseconf.outerinterfaceip,g_baseconf.outerinterfacemask);
//	system(cmd);
//	strcpy(g_baseconf.innerinterfaceip,g_baseconf.outerinterfaceip);
#else

    //copy the ip to /etc/interface or  /etc/network
    //

#endif


}

void init_baseconf() {
    bzero(&g_baseconf, sizeof(g_baseconf));
    strcpy(g_baseconf.propertyip, "11.0.0.1");
    strcpy(g_baseconf.streamserverip, "11.0.0.1");
    strcpy(g_baseconf.alarmcenterip, "11.0.0.2");
    strcpy(g_baseconf.callcenterip, "11.0.0.3");
    strcpy(g_baseconf.outerboxip, "0.0.0.0");
}

void write_seczone_conf_file(void);

void init_gseczone_mode_conf();

void init_seczone_conf_t();

cJSON *config_reset(jrpc_context *ctx, cJSON *params, cJSON *id) {

    //reset extend conf
    init_gseczone_mode_conf();
    init_seczone_conf_t();
    strcpy(g_seczone_passwd, "0000");

    g_seczone_count = 8;
    g_seczone_mode = 0;

    init_baseconf();

    write_seczone_conf_file();
    create_extend_conf_file();
    create_basic_conf_file();
    char cmd[256] = {0};
    snprintf(cmd, 256, "rm -rf %s %s %s %s %s %s %s %s", INNER_DB, SECMSG_FILE, SECMSG_FILE_T, INNERBOX_CONF,
             INNERBOX_CONF_BAK, SECZONE_CONF_BAK, BASIC_CONF_BAK, EXTEND_CONF_BAK);
    system(cmd);

#ifdef SQL
    create_sqlite_table();
#endif
    system("reboot");

    return cJSON_CreateString("config_reset_has_done");
}

void reset_app_state();

//engineering setup when get request from app
cJSON *init_setup(jrpc_context *ctx, cJSON *params, cJSON *id) {
    cJSON *param = NULL;
/*
	if(access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
*/
    if (params == NULL)
        return cJSON_CreateString("init setup null params!");

    G_CONF_LOCK;

    //process params
    param = cJSON_GetObjectItem(params, "outerbox_f1");
    if (param != NULL)
        if (param->type == cJSON_String) {
            strcpy(g_baseconf.outerboxip, param->valuestring);
            // scott 2018.12.3
            strcpy(g_baseconf.outerinterfaceip, param->valuestring);
        }

    param = cJSON_GetObjectItem(params, "outerbox_b1");
    if (param != NULL)
        if (param->type == cJSON_String)
            strcpy(g_baseconf.outerboxip_1, param->valuestring);

    param = cJSON_GetObjectItem(params, "gateouterbox");
    if (param != NULL)
        if (param->type == cJSON_String)
            strcpy(g_baseconf.gateouterboxip, param->valuestring);

    param = cJSON_GetObjectItem(params, "localipaddr");
    if (param != NULL)
        if (param->type == cJSON_String)
            strcpy(g_baseconf.innerinterfaceip, param->valuestring);

/*
	param = cJSON_GetObjectItem(params, "localipaddr");
	if(param != NULL)
		if(param->type == cJSON_String)
			strcpy(g_baseconf.outerinterfaceip,param->valuestring);

	param = cJSON_GetObjectItem(params, "localnetmask");
	if(param != NULL)
		if(param->type == cJSON_String)
			strcpy(g_baseconf.outerinterfacemask,param->valuestring);
*/
    param = cJSON_GetObjectItem(params, "propertyserver");
    if (param != NULL)
        if (param->type == cJSON_String)
            strcpy(g_baseconf.propertyip, param->valuestring);

    param = cJSON_GetObjectItem(params, "callcenter");
    if (param != NULL)
        if (param->type == cJSON_String)
            strcpy(g_baseconf.callcenterip, param->valuestring);

    param = cJSON_GetObjectItem(params, "alarmcenter");
    if (param != NULL)
        if (param->type == cJSON_String)
            strcpy(g_baseconf.alarmcenterip, param->valuestring);

    param = cJSON_GetObjectItem(params, "streamserver");
    if (param != NULL)
        if (param->type == cJSON_String)
            strcpy(g_baseconf.streamserverip, param->valuestring);

//	DEBUG_INFO("init ip:%s %s %s",g_baseconf.outerboxip,g_baseconf.outerinterfaceip,g_baseconf.propertyip);
    printf("== init setup() , outerboxip_f1:%s,outerbox_b1:%s\n", g_baseconf.outerboxip, g_baseconf.outerboxip_1);
    //do the setup
    do_set_ip();

    create_basic_conf_file();
    create_extend_conf_file();
    //
    //ip conf change
    //reget the ipconf from server
    if (g_ipconf_get_flag == 1) {
        g_ipconf_get_flag = 0;
        create_worker(ipaddr_query_from_property, 0);
    }

    reset_app_state();

    G_CONF_UNLOCK;

    return cJSON_CreateString("init_setup_has_done");

}

//get the ipconf of backend
//when app enter init_setup interface
cJSON *ipconf_get(jrpc_context *ctx, cJSON *params, cJSON *id) {

    char jsonstr[1024];
    bzero(jsonstr, 1024);

    snprintf(jsonstr, 1024,
             "{\"result\":\"ipconf_of_innerbox\",\"outerbox_f1\":\"%s\",\"outerbox_b1\":\"%s\",\"gateouterbox\":\"%s\",\"propertyserver\":\"%s\",\"callcenter\":\"%s\",\"alarmcenter\":\"%s\",\"streamserver\":\"%s\"}^",
             g_baseconf.outerboxip,
             g_baseconf.outerboxip_1,
             g_baseconf.gateouterboxip,
//				g_baseconf.outerinterfaceip,
//				g_baseconf.outerinterfacemask,
             g_baseconf.propertyip,
             g_baseconf.callcenterip,
             g_baseconf.alarmcenterip,
             g_baseconf.streamserverip);

    send_app_data(jsonstr,strlen(jsonstr));

//    app_client_t *procer = get_appt_by_ctx(ctx);
//    if (procer) {
//        write(((struct jrpc_connection *) (ctx->data))->fd, jsonstr, strlen(jsonstr));
//    }
    return NULL;
}

cJSON *msgpush_to_app() {
    char urlstring[1024];
    GET_LOCAL_ADDRESS();

    cJSON *result_root = cJSON_CreateObject();
    cJSON *string = cJSON_CreateString("msgpush");
    cJSON_AddItemToObject(result_root, "method", string);
    cJSON *content_obj = cJSON_CreateObject();
    string = cJSON_CreateString("天气预报");
    cJSON_AddItemToObject(content_obj, "title", string);
    string = cJSON_CreateNumber(1);
    cJSON_AddItemToObject(content_obj, "level", string);
    sprintf(urlstring, "http://%s/index.html", g_baseconf.innerinterfaceip);
    //string = cJSON_CreateString("http://192.168.1.137/index.html");
    string = cJSON_CreateString(urlstring);
    cJSON_AddItemToObject(content_obj, "url", string);
    cJSON_AddItemToObject(result_root, "content", content_obj);

    return result_root;
}

//push message to app
cJSON *msg_push(jrpc_context *ctx, cJSON *params, cJSON *id) {
    int i = 0;
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();

    int ret = 0;
    char *msgstr;
    char *tmpstr;
#ifdef ALL_IN_ONE
                                                                                                                            //just resend the packet from property to app
    cJSON *result_root = cJSON_CreateObject();
    cJSON *string = cJSON_CreateString("msgpush");
    cJSON_AddItemToObject(result_root,"method",string);
	cJSON_AddItemToObject(result_root,"params",params);
	msgstr = cJSON_Print(result_root);
	tmpstr = malloc(strlen(msgstr)+2);
	memset(tmpstr,0,strlen(msgstr)+2);
	snprintf(tmpstr,strlen(msgstr)+2,"%s^",msgstr);
	cJSON_Delete(result_root);
#else
    //need to change the url or the www server ip addr
    //because the app can not connect to the property directly
    //not implemented
/*	cJSON * msgobj = msgpush_to_app();
	msgstr = cJSON_Print(msgobj);
	tmpstr = malloc(strlen(msgstr)+2);
	memset(tmpstr,0,strlen(msgstr)+2);
	strcpy(tmpstr,msgstr);
	strncat(tmpstr,"^",1);

	cJSON_Delete(msgobj);*/
#endif
//    while (appt) {
        check_and_set_app_offline(appt);

        if (appt->current_state >= JOINED) {
            int lport = get_local_sockinfo(appt->socket_fd);
            if (lport > 0 && lport == IN_PORT) {
                ret = write(appt->socket_fd, tmpstr, strlen(tmpstr));
            } else
                appt->current_state = OFFLINE;
            //ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
            DEBUG_INFO("msg write:%s %d", appt->ip_addr_str, ret);
        }

        if (ret == -1)
            appt->current_state = OFFLINE;

//        appt = appt->next;
//    }

    free(msgstr);//?????need it
    free(tmpstr);

    return cJSON_CreateString("msg_has_push_to_app");
}

cJSON *updatemsg_to_app(char *devtype, char *urlstring, char *version, char *updatetime) {

    cJSON *updatemsg = cJSON_CreateObject();
    cJSON *string = cJSON_CreateString("update_push");
    cJSON_AddItemToObject(updatemsg, "method", string);
    cJSON *param_obj = cJSON_CreateObject();
    string = cJSON_CreateString(devtype);
    cJSON_AddItemToObject(param_obj, "devtype", string);
    string = cJSON_CreateString(urlstring);
    cJSON_AddItemToObject(param_obj, "url", string);
    string = cJSON_CreateString(version);
    cJSON_AddItemToObject(param_obj, "version", string);
    string = cJSON_CreateString(updatetime);
    cJSON_AddItemToObject(param_obj, "updatetime", string);
    cJSON_AddItemToObject(updatemsg, "params", param_obj);

    return updatemsg;
}

void *do_softupdate() {
    pthread_detach(pthread_self());
    char cmdstr[512];
    bzero(cmdstr, 512);
    //wget the update package
#ifdef ANDROID
                                                                                                                            chdir("/data/local/tmp/");
	snprintf(cmdstr,512,"wget -O inner_update_package.tgz %s",g_urlstring);
	system(cmdstr);
	rc4_crypt_file("inner_update_package.tgz","inner_update_package_dec.tgz");
	system("tar zxvf inner_update_package_dec.tgz");
	system("rm -rf inner_update_package*.tgz");
	system("/data/local/tmp/inner_update.sh");
	system("sync");
#else
    chdir("/");
    snprintf(cmdstr, 512, "wget -O inner_update_package.tgz %s", g_urlstring);
    system(cmdstr);
    rc4_crypt_file("inner_update_package.tgz", "inner_update_package_dec.tgz");
    system("tar zxvf inner_update_package_dec.tgz");
    system("rm -rf inner_update_package*.tgz");
    system("/tmp/inner_update.sh");
    system("sync");
#endif
}

cJSON *softupdate_innerbox(jrpc_context *ctx, cJSON *params, cJSON *id) {

    cJSON *param = NULL;
    char version[32];
    bzero(version, 32);
#ifdef DEBUG
                                                                                                                            char * str = cJSON_Print(params);
	DEBUG_INFO("update:%s",str);
	free(str);
#endif
    bzero(g_urlstring, 1024);
    param = cJSON_GetObjectItem(params, "version");
    if (param != NULL)
        if (param->type == cJSON_String)
            strncpy(version, param->valuestring, 32);
        else
            return cJSON_CreateString(WRONG_PARAMETER);
    else
        return cJSON_CreateString(WRONG_PARAMETER);

    if (strcmp(g_baseconf.version, version) == 0) {
        return cJSON_CreateString("Version_is_same_as_current");
    }

    param = cJSON_GetObjectItem(params, "url");
    if (param != NULL)
        if (param->type == cJSON_String)
            strncpy(g_urlstring, param->valuestring, 1024);
        else
            return cJSON_CreateString(WRONG_PARAMETER);
    else
        return cJSON_CreateString(WRONG_PARAMETER);

    g_update_state = 1;

    create_worker(do_softupdate, NULL);
    return cJSON_CreateString("doing_softupdate_for_innerbox_just_waiting");
}

//get the update push message of
//{"method":"update_push","params":{"type":"innerbox/outerbox/android-pad/android-phone/ios-pad/ios-phone","url":"http://x.x.x.x/package","filename":"fff.tgz","version":"1.1","updatetime":"2017-10-1 12:00:00"}}
cJSON *update_push(jrpc_context *ctx, cJSON *params, cJSON *id) {

    cJSON *param = NULL;
    cJSON *msgobj = NULL;
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
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

    memset(devtype, 0, 16);
    memset(urlstring, 0, 1024);
    memset(version, 0, 32);
    memset(updatetime, 0, 32);
    memset(insertsql, 0, 1024);
    memset(filename, 0, 512);
    memset(cmdstr, 0, 512);

    if (params == NULL)
        return cJSON_CreateString("update push null params!");
    //process params
    param = cJSON_GetObjectItem(params, "type");
    if (param != NULL)
        if (param->type == cJSON_String)
            strncpy(devtype, param->valuestring, 16);
        else
            return cJSON_CreateString(WRONG_PARAMETER);
    else
        return cJSON_CreateString(WRONG_PARAMETER);

    param = cJSON_GetObjectItem(params, "url");
    if (param != NULL)
        if (param->type == cJSON_String)
            strncpy(urlstring, param->valuestring, 1024);
        else
            return cJSON_CreateString(WRONG_PARAMETER);
    else
        return cJSON_CreateString(WRONG_PARAMETER);

    param = cJSON_GetObjectItem(params, "filename");
    if (param != NULL)
        if (param->type == cJSON_String)
            strncpy(filename, param->valuestring, 512);
        else
            return cJSON_CreateString(WRONG_PARAMETER);
    else
        return cJSON_CreateString(WRONG_PARAMETER);

    param = cJSON_GetObjectItem(params, "version");
    if (param != NULL)
        if (param->type == cJSON_String)
            strncpy(version, param->valuestring, 32);
        else
            return cJSON_CreateString(WRONG_PARAMETER);
    else
        return cJSON_CreateString(WRONG_PARAMETER);

    param = cJSON_GetObjectItem(params, "updatetime");
    if (param != NULL)
        if (param->type == cJSON_String)
            strncpy(updatetime, param->valuestring, 32);
        else
            return cJSON_CreateString(WRONG_PARAMETER);
    else
        return cJSON_CreateString(WRONG_PARAMETER);

    //insert to database

    snprintf(insertsql, 1024, "insert into updateinfo values ('%s','%s', '%s', '%s')", devtype, version, updatetime,
             filename);
    DEBUG_INFO("%s", insertsql);
#ifdef SQL
                                                                                                                            ret = insert_data_to_db(insertsql);
	if(ret != 0) {
		snprintf(insertsql,1024,"update updateinfo set filename = '%s', version='%s',updatetime = '%s' where devtype = '%s'",filename, version, updatetime,  devtype);
		insert_data_to_db(insertsql);
	}
#endif
    msgobj = updatemsg_to_app(devtype, urlstring, version, updatetime);
    msgstr = cJSON_Print(msgobj);
    tmpstr = malloc(strlen(msgstr) + 2);
    snprintf(tmpstr, strlen(msgstr) + 2, "%s^", msgstr);

//    while (appt) {
        if (strcmp(appt->type, devtype) == 0) {
//            char raddress[128];
//            bzero(raddress, 128);
//            int rport = get_peer_address(appt->socket_fd, raddress);
//            if (rport != appt->port || strcmp(appt->ip_addr_str, raddress) != 0) {
//                appt = appt->next;
//                continue;
//            }

            if (appt->current_state >= JOINED) {
                int lport = get_local_sockinfo(appt->socket_fd);
                if (lport > 0 && lport == IN_PORT) {
                    ret = write(appt->socket_fd, tmpstr, strlen(tmpstr));
                } else
                    appt->current_state = OFFLINE;
                if (ret == -1)
                    appt->current_state = OFFLINE;
            }
        }

//        appt = appt->next;
//    }

    cJSON_Delete(msgobj);
    free(msgstr);
    free(tmpstr);

    //type = innerbox
    if (strcmp(devtype, "innerbox") == 0) {
        chdir("/");
        snprintf(cmdstr, 512, "wget %s", urlstring);
        system(cmdstr);
        rc4_crypt_file(filename, "test.tgz");
        system("ls -l");
    }
    return cJSON_CreateString("update_push_has_done");
}

#ifdef ALL_IN_ONE
#define OUTERBOXIP outerboxip
#else
#define OUTERBOXIP g_baseconf.innerinterfaceip
#endif

void generate_outerbox_video_streamurl(char *stream_outerbox, char *outerboxip) {

    if (strcmp(outerboxip, g_baseconf.outerboxip) == 0)
        snprintf(stream_outerbox, 1024, g_baseconf.outerboxvideostreamurl, OUTERBOXIP);

    if (strcmp(outerboxip, g_baseconf.outerboxip_1) == 0)
        snprintf(stream_outerbox, 1024, g_baseconf.outerboxvideostreamurl_1, OUTERBOXIP);

    if (strcmp(outerboxip, g_baseconf.gateouterboxip) == 0)
        snprintf(stream_outerbox, 1024, g_baseconf.gateouterboxvideostreamurl, OUTERBOXIP);

    // scott
    if (strlen(stream_outerbox) == 0) {
        snprintf(stream_outerbox, 1024, g_baseconf.gateouterboxvideostreamurl, outerboxip);
    }
}

cJSON *opendoorcmd_to_app(app_client_t *appt, char *outerboxip) {
    //char urlstring[1024];
    char stream_outerbox[1024];
    cJSON *result_root = cJSON_CreateObject();
    cJSON *string = cJSON_CreateString("opendoorplease");
    cJSON_AddItemToObject(result_root, "method", string);
    cJSON *param_obj = cJSON_CreateObject();
    string = cJSON_CreateString("001");
    cJSON_AddItemToObject(param_obj, "doorid", string);

    GET_LOCAL_ADDRESS();
    //sprintf(urlstring,"rtsp://%s:554/ch01_sub.h264",g_baseconf.innerinterfaceip);
    //sprintf(urlstring,"rtsp://%s:554/user=admin_password=tlJwpbo6_channel=1_stream=1.sdp?real_stream",g_baseconf.innerinterfaceip);
    //sprintf(urlstring,"rtsp://admin:admin@%s:554/stream1",g_baseconf.innerinterfaceip);
    generate_outerbox_video_streamurl(stream_outerbox, outerboxip);

    { // scott
        char randstr[10];
        memset(randstr, 0, 10);
        get_rand_str(randstr, 9);
        generate_audio_stream(outerboxip, appt->audiostream_push_url_outer, randstr);
    }

    string = cJSON_CreateString(stream_outerbox);
    cJSON_AddItemToObject(param_obj, "video", string);
//        string = cJSON_CreateString(appt->audiostream_url);
    string = cJSON_CreateString(appt->audiostream_push_url_outer); // scott

    cJSON_AddItemToObject(param_obj, "audio", string);
    cJSON_AddItemToObject(result_root, "params", param_obj);

    return result_root;
}

void *generate_outerbox_callcmdstr_to_app(app_client_t *appt, char *outerboxip, char *cmdstr) {

    cJSON *opendoor = opendoorcmd_to_app(appt, outerboxip);
    char *openstr = cJSON_Print(opendoor);

    snprintf(cmdstr, 2048, "%s^", openstr);

    free(openstr);
    cJSON_Delete(opendoor);

    printf("== (generate_outerbox_callcmdstr_to_app) make cmd to app . %s \n", cmdstr); // scott
}

void callhistory_log_to_db(char *time, char *person, char *event, char *peertype, char *peerid) {
    char sqlstr[1024];
    memset(sqlstr, 0, 1024);
    if (!time || !person || !event || !peertype || !peerid)
        return;
    snprintf(sqlstr, 1024, "insert into callhistory values ('%s','%s','%s','%s','%s')", time, person, event, peertype,
             peerid);
#ifdef SQL
    insert_data_to_db(sqlstr);
#endif
}

void check_and_set_app_offline(app_client_t *appt) {
//    char raddress[64];
//    bzero(raddress, 64);
//
//    int rport = get_peer_address(appt->socket_fd, raddress);
//    if (rport != appt->port || strcmp(appt->ip_addr_str, raddress) != 0) {
//        appt->current_state = OFFLINE;
//    }
}

/*
#define UF 1
#define GF 2
#define BF 4
#define IF 8
#define OF 16
#define PF 32
#define SF 64
#define CF 128
#define AF 256
#define AAF 256*2
#define UVF 256*4
#define BVF 256*8
#define GVF 256*16
*/
int conf_is_ready(char *msg) {
    int flag = 0;

    if (strlen(g_baseconf.outerboxip) == 0) {
        flag |= UF;
        if (msg != NULL)
            strcat(msg, "单元机地址未配置;");
    }

    if (strlen(g_baseconf.gateouterboxip) == 0) {
        flag |= GF;
        if (msg != NULL)
            strcat(msg, "小区门口室外机地址未配置;");
    }

    if (strlen(g_baseconf.outerboxip_1) == 0) {
        flag |= BF;
        if (msg != NULL)
            strcat(msg, "负一楼单元机地址未配置;");
    }

    if (strlen(g_baseconf.innerinterfaceip) == 0) {
        flag |= IF;
        if (msg != NULL)
            strcat(msg, "本机内部地址未配置;");
    }

    if (strlen(g_baseconf.outerinterfaceip) == 0) {
        flag |= OF;
        if (msg != NULL)
            strcat(msg, "本机外部地址未配置;");
    }

    if (strlen(g_baseconf.propertyip) == 0) {
        flag |= PF;
        if (msg != NULL)
            strcat(msg, "物业服务器地址未配置;");
    }

    if (strlen(g_baseconf.streamserverip) == 0) {
        flag |= SF;
        if (msg != NULL)
            strcat(msg, "推流服务器地址未配置;");
    }

    if (strlen(g_baseconf.callcenterip) == 0) {
        flag |= CF;
        if (msg != NULL)
            strcat(msg, "呼叫中心地址未配置;");
    }

    if (strlen(g_baseconf.alarmcenterip) == 0) {
        flag |= AF;
        if (msg != NULL)
            strcat(msg, "报警处理中心地址未配置;");
    }

    if (strlen(g_baseconf.appaudiostreamurl) == 0) {
        flag |= AAF;
        if (msg != NULL)
            strcat(msg, "APP音频推流URL未配置;");
    }

    if (strlen(g_baseconf.outerboxvideostreamurl) == 0) {
        flag |= UVF;
        if (msg != NULL)
            strcat(msg, "单元机视频流URL未配置;");
    }

    if (strlen(g_baseconf.outerboxvideostreamurl_1) == 0) {
        flag |= BVF;
        if (msg != NULL)
            strcat(msg, "负一楼单元机视频流URL未配置;");
    }

    if (strlen(g_baseconf.gateouterboxvideostreamurl) == 0) {
        flag |= GVF;
        if (msg != NULL)
            strcat(msg, "小区门口机视频流URL未配置;");
    }

    return flag;
}

// return : 1 - busy , 0 - idle
// 配置参数未同步配置完成也显示忙
int app_is_busy() {
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
    int busy = 0;

    if (conf_is_ready(NULL) != 0)
        return 1;

//    while (appt) {
        check_and_set_app_offline(appt);

        if (appt->current_state >= CONNECTING) {
            busy = 1;
        }
//        appt = appt->next;
//    }

    return busy;
}

cJSON *open_door(jrpc_context *ctx, cJSON *params, cJSON *id) {

    int i = 0;
    int ret = 0;
    cJSON *param = NULL;
    char cmdstr[1024];
    time_t now;
    char outerboxip[64];

    printf("== (open_door) .. \n");

    bzero(outerboxip, 64);
    if (params != NULL)
        param = cJSON_GetObjectItem(params, "ipaddr");
    else
        return cJSON_CreateString(WRONG_PARAMETER);

    if (param != NULL)
        if (param->type == cJSON_String)
            strncpy(outerboxip, param->valuestring, 64);
        else
            return cJSON_CreateString(WRONG_PARAMETER);
    //line is busy

    char timestr[64];
    memset(cmdstr, 0, 1024);
    memset(timestr, 0, 64);
    int busy = 0;
    char raddress[128];
    bzero(raddress, 128);
    char peeraddress[128];
    bzero(peeraddress, 128);

    int outerport = get_peer_address(((struct jrpc_connection *) (ctx->data))->fd, peeraddress);
    if (g_nocallin_delay > 0 || app_is_busy() ==
                                1 /*||conf_is_ready(NULL)&UF == UF || conf_is_ready(NULL)&GF == GF || conf_is_ready(NULL)&BF == BF*/) {
        //log
        callhistory_log_to_db(timestr, peeraddress, "呼入占线", "单元机", peeraddress);

        return cJSON_CreateString(LINE_BUSY);
    }
    printf("== open_door : peer(%s), outerboxip: %s \n", peeraddress, outerboxip);

    DEBUG_INFO("open_door from :%s", peeraddress);
    busy = select_idle_app_and_send_cmd(OUTER2APP, peeraddress, outerport, ((struct jrpc_connection *) (ctx->data))->fd,
                                        outerboxip);
    if (busy == 0)
        return cJSON_CreateString(LINE_BUSY);

    printf("== busy code: %d\n", busy);

    //do history record
    //may write it to database
    //grab a picture of the caller
    memset(g_last_caller, 0, 64);
    snprintf(g_last_caller, 64, "%d.jpg", (int) time(&now));
    //snprintf(cmdstr,1024,"ffmpeg -y -i %s -vframes 1 %s%s &",g_stream_outerbox,CALLHISTORY_PATH,g_last_caller);
    //system(cmdstr);
    get_time_now(timestr);
    //callhistory_log_to_db(timestr,g_last_caller,"请求开门");
    callhistory_log_to_db(timestr, g_last_caller, "呼入", "单元机", peeraddress);
    //close(((struct jrpc_connection*)(ctx->data))->fd);
    return cJSON_CreateString("opendoor_request_sent");
}

cJSON *say_hello(jrpc_context *ctx, cJSON *params, cJSON *id) {
    return cJSON_CreateString("Hello!");
}

app_client_t *malloc_init_app_client_t(void) {
    app_client_t *appt = malloc(sizeof(app_client_t));
    bzero(appt, sizeof(app_client_t));

    appt->line_state = malloc(sizeof(call_line_state_t));
    bzero(appt->line_state, sizeof(call_line_state_t));
    return appt;
}

//update info of APP client
// todo.
app_client_t *
update_app_client_info(int fd, const char *peeraddr, int port, char *app_dev_id, int state, char *name, char *type) {
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
//    app_client_t *newappt = 0;
//    app_client_t *lastappt = 0;
//    int find = 0;
//    char insertsql[1024];
//    char datetime[200];
//    int ret = 0;

//    if (appt) {
//        while (appt != NULL) {
//            if (appt->socket_fd == fd) {
                appt->socket_fd = fd;
                if (peeraddr != NULL)
                    strcpy(appt->ip_addr_str, peeraddr);
                if (port > 0)
                    appt->port = port;
                appt->current_state = state;
                if (name != NULL)
                    strcpy(appt->name, name);
                if (type != NULL)
                    strcpy(appt->type, type);
                if (app_dev_id != NULL)
                    strcpy(appt->app_dev_id, app_dev_id);
//                find++;
//                newappt = appt;
//            }
//            lastappt = appt;
//            appt = appt->next;
//        }
//    }

//    if (find == 0) {
//        newappt = malloc_init_app_client_t();
//        newappt->socket_fd = fd;
//        newappt->port = port;
//        if (app_dev_id != NULL)
//            strcpy(newappt->app_dev_id, app_dev_id);
//        if (peeraddr != NULL)
//            strcpy(newappt->ip_addr_str, peeraddr);
//        newappt->current_state = state;
//        if (name != NULL)
//            strcpy(newappt->name, name);
//        if (type != NULL)
//            strcpy(newappt->type, type);
//        if (!lastappt) {
//            g_app_client_list = newappt;
//        } else
//            lastappt->next = newappt;
//    }

    //insert the appinfo to database
//    if (newappt->current_state >= JOINED) {
//
//        memset(insertsql, 0, 1024);
//        memset(datetime, 0, 200);
//        get_time_now(datetime);
//        snprintf(insertsql, 1024, "insert into appinfo values ('%s','%s', '%s', '%s')", newappt->app_dev_id, name,
//                 newappt->type, datetime);
//        DEBUG_INFO("%s", insertsql);
//#ifdef SQL
//                                                                                                                                ret = insert_data_to_db(insertsql);
//		if(ret != 0) {
//			snprintf(insertsql,1024,"update appinfo set name = '%s', type='%s',joindate = '%s' where appid = '%s'",newappt->name,newappt->type,datetime,  newappt->app_dev_id);
//			insert_data_to_db(insertsql);
//		}
//#endif
//    }

//    return newappt;
    return appt;
}

void travel_app_list() {
//    app_client_t *appt = g_app_client_list;
//    app_client_t *appt = get_app_client();
//    while (appt != NULL) {
//        DEBUG_INFO("app id:%s ip:%s port:%d state:%d socket:%d ", appt->app_dev_id, appt->ip_addr_str, appt->port,
//                   appt->current_state, appt->socket_fd);
//        appt = appt->next;
//    }
}

void get_local_address() {

    struct ifaddrs *ifAddrStruct = NULL;
    struct ifaddrs *ifa = NULL;
    void *tmpAddrPtr = NULL;
    if (getifaddrs(&ifAddrStruct) == -1) {  // scott 此函数会报异常
//		strcpy(g_baseconf.innerinterfaceip,"127.0.0.1");
//		strcpy(g_baseconf.outerinterfaceip,"127.0.0.1");
        return;
    }
    ifa = ifAddrStruct;
    while (ifAddrStruct != NULL) {
        if (ifAddrStruct->ifa_addr == NULL) {
            ifAddrStruct = ifAddrStruct->ifa_next;
            continue;
        }

//		printf("ifAddrStruct:%d \n",ifAddrStruct);
        //	printf("ifAddrStruct->ifa_addr:%d \n",ifAddrStruct->ifa_addr);
        //	printf("ifAddrStruct->ifa_addr->sa_family:%d \n",ifAddrStruct->ifa_addr->sa_family);
        if (ifAddrStruct->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr = &((struct sockaddr_in *) ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            bzero(addressBuffer, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            DEBUG_INFO("%s IP Address %s", ifAddrStruct->ifa_name, addressBuffer);

            if (strcmp(ifAddrStruct->ifa_name, g_baseconf.innerinterfacename) == 0) {
                strcpy(g_baseconf.innerinterfaceip, addressBuffer);
                tmpAddrPtr = &((struct sockaddr_in *) ifAddrStruct->ifa_netmask)->sin_addr;
                bzero(addressBuffer, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                strcpy(g_baseconf.innerinterfacemask, addressBuffer);
                break;
            }
#ifndef ALL_IN_ONE
            else if (strcmp(ifAddrStruct->ifa_name, g_baseconf.outerinterfacename) == 0) {
                strcpy(g_baseconf.outerinterfaceip, addressBuffer);
                tmpAddrPtr = &((struct sockaddr_in *) ifAddrStruct->ifa_netmask)->sin_addr;
                bzero(addressBuffer, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                strcpy(g_baseconf.outerinterfacemask, addressBuffer);
            }
#endif

        } else if (ifAddrStruct->ifa_addr->sa_family == AF_INET6) {
            tmpAddrPtr = &((struct sockaddr_in *) ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            DEBUG_INFO("%s IP Address %s", ifAddrStruct->ifa_name, addressBuffer);
            /*		if(strncmp(ifAddrStruct->ifa_name,"eth0",4) == 0)
				strcpy(g_baseconf.innerinterfaceip,addressBuffer);
			if(strncmp(ifAddrStruct->ifa_name,"eth1",4) == 0)
				strcpy(g_baseconf.outerinterfaceip,addressBuffer);      */
        }
        ifAddrStruct = ifAddrStruct->ifa_next;
    }
    freeifaddrs(ifa);
#ifdef ALL_IN_ONE
                                                                                                                            //if(strlen(g_baseconf.outerinterfaceip) == 0) {
	if(1) {
		strcpy(g_baseconf.outerinterfaceip,g_baseconf.innerinterfaceip);
		strcpy(g_baseconf.outerinterfacemask,g_baseconf.innerinterfacemask);
	}
#endif
}

int get_peer_address(int fd, char *peeraddress) {
    //struct sockaddr_in  peerAddr;//连接的对端地址
    struct sockaddr_storage peerAddr;//连接的对端地址
    socklen_t peerLen = sizeof(peerAddr);
    int ret = 0;
    char ipstr[128];
    bzero(ipstr, 128);
    int port = 0;
    char *ptr = NULL;
    memset(&peerAddr, 0, sizeof(peerAddr));
    //get the app client addr
    ret = getpeername(fd, (struct sockaddr *) &peerAddr, &peerLen);
    if (ret == -1) perror("getpeername error!");
    if (peerAddr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *) &peerAddr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
        strcpy(peeraddress, ipstr);
    } else if (peerAddr.ss_family == AF_INET6) {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *) &peerAddr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
        ptr = strtok(ipstr, ":");
        while (ptr != NULL) {
            strcpy(peeraddress, ptr);
            ptr = strtok(NULL, ":");
        }
    }

    //strcpy(peeraddress,ipstr);
    DEBUG_INFO("connected peer address = %s:%d", peeraddress, port);

    return port;
}

cJSON *packet42_create(char *qrcode, char *name, char *type) {
    cJSON *result_root = cJSON_CreateObject();
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "qrcode", qrcode);
    cJSON_AddStringToObject(params, "name", name);
    cJSON_AddStringToObject(params, "type", type);
    cJSON_AddStringToObject(result_root, "method", "join_family_request");
    cJSON_AddItemToObject(result_root, "params", params);

    return result_root;
}

//APP dial in control
cJSON *join_family(jrpc_context *ctx, cJSON *params, cJSON *id) {
    cJSON *param = NULL;
    char peeraddress[64];
    memset(peeraddress, 0, 64);
    int peerport = 0;
    char qrcode[512];
    memset(qrcode, 0, 512);
    char type[16];
    memset(type, 0, 16);
    char sqlstr[1024];
    memset(sqlstr, 0, 1024);
    char name[64];
    memset(name, 0, 64);
    char version[32];
    memset(version, 0, 32);
    int ret = 0;
    int fd = ((struct jrpc_connection *) (ctx->data))->fd;
    cJSON *packet42 = NULL;
    char *packet42_str;
    char *tmpstr;
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
    char sentrybox[2048];
    bzero(sentrybox, 2048);

    char tstr[256];
    int i = 0;
    char cmdstr[2048];

    strcpy(sentrybox, "[");
    for (i = 0; i < g_baseconf.sentryboxcount; i++) {
        bzero(tstr, 256);
        snprintf(tstr, 256, "{\"name\":\"%s\",\"ipaddr\":\"%s\"}", g_baseconf.sentrybox[i].name,
                 g_baseconf.sentrybox[i].ipaddr);
        strncat(sentrybox, tstr, 256);
        if (i < g_baseconf.sentryboxcount - 1)
            strcat(sentrybox, ",");
    }
    strcat(sentrybox, "]");


    peerport = get_peer_address(fd, peeraddress);

    GET_LOCAL_ADDRESS();

    if (params == NULL)
        return cJSON_CreateString("join family null params!");

    param = cJSON_GetObjectItem(params, "qrcode");
    if (param != NULL && param->type == cJSON_String) {
        strcpy(qrcode, param->valuestring);
    } else
        return cJSON_CreateString("family_join_deny");

    //process params
    param = cJSON_GetObjectItem(params, "type");
    if (param != NULL && param->type == cJSON_String)
        strcpy(type, param->valuestring);
    else
        return cJSON_CreateString("family_join_deny");

    param = cJSON_GetObjectItem(params, "name");
    if (param != NULL && param->type == cJSON_String)
        strcpy(name, param->valuestring);

    param = cJSON_GetObjectItem(params, "version");
    if (param != NULL && param->type == cJSON_String)
        strcpy(version, param->valuestring);
    if (strlen(qrcode) > 0) {
        snprintf(sqlstr, 1024, "select * from appinfo where appid='%s'", qrcode);
#ifdef SQL
        ret = query_appinfo_fromdb(sqlstr);
#endif
    }
    //randstr generate
    char randstr[10];
    memset(randstr, 0, 10);

    get_rand_str(randstr, 9);
    //init_stream_url();
    //ret = 1; // for test 2017.03.05
    //find the qrcode
//    if (ret == 1) {
    if (1) {
//        joinpermit:
        DEBUG_INFO("family_join_permit:%s", peeraddress);

        app_client_t *appt = update_app_client_info(fd, peeraddress, peerport, qrcode, ONLINE, name, type);
        strcpy(appt->version, version);

        bzero(appt->audiostream_url, 1024);
        bzero(appt->audiostream_out, 1024);

        if (strlen(g_baseconf.streamserverip) == 0 ||
            strcmp(g_baseconf.innerinterfaceip, "127.0.0.1") == 0 ||
            strcmp(g_baseconf.outerinterfaceip, "127.0.0.1") == 0) {
            return cJSON_CreateString("family_join_waiting");
        }

#ifdef ALL_IN_ONE
                                                                                                                                //generate_audio_stream(g_baseconf.outerboxip,appt->audiostream_url,randstr);
		generate_audio_stream(g_baseconf.streamserverip,appt->audiostream_url,randstr);
#else
//        generate_audio_stream(g_baseconf.innerinterfaceip, appt->audiostream_url, randstr);
#endif
        //generate_audio_stream(g_baseconf.outerboxip,appt->audiostream_out,randstr);
        generate_audio_stream(g_baseconf.streamserverip, appt->audiostream_out, randstr);

        DEBUG_INFO("#####%s %s", appt->audiostream_url, appt->audiostream_out);

//		innerbox_info_t * innerbox_t = select_from_innerbox_info_list_byip(g_baseconf.outerinterfaceip); //scott 阿福搞错了，弄成室外机的了
        innerbox_info_t *innerbox_t = select_from_innerbox_info_list_byip(g_baseconf.innerinterfaceip);
        if (innerbox_t == NULL) {
            printf("== join_family() no innerbox matched by ip: %s\n", g_baseconf.innerinterfaceip);
        }
//        if (innerbox_t) {
		if(1){
            bzero(cmdstr, 2048);
            if( innerbox_t) {
				snprintf(cmdstr, 2048,
						 "{\"result\":\"family_join_permit\",\"doorid\":\"%s\",\"audiourl\":\"%s\",\"onekeyset\":\"%d\",\"sentrybox\":%s}^",
						 innerbox_t->doorid,/*innerbox_t->ipaddr,*/appt->audiostream_url, g_onekey_set, sentrybox);
			}else{
				snprintf(cmdstr, 2048,
						 "{\"result\":\"family_join_permit\",\"doorid\":\"%s\",\"audiourl\":\"%s\",\"onekeyset\":\"%d\",\"sentrybox\":%s}^",
						 "99-99-99",/*innerbox_t->ipaddr,*/appt->audiostream_url, g_onekey_set, sentrybox);
            }
            send_app_data(cmdstr, strlen(cmdstr));
            update_app_client_info(fd, peeraddress, peerport, qrcode, JOINED, name, type);
            on_event_app_joined(fd);
        }
//        else {
//            printf("== close socket with app. \n");
//            close(appt->socket_fd);
//        }
        return NULL;
        //return cJSON_CreateString("family_join_permit");
    }
    /*
    else {
        if (strcmp(type, "android-pad") == 0) {
            goto joinpermit;
            //return cJSON_CreateString("family_join_permit");
        } else if (strcmp(type, "ios-phone") == 0 || strcmp(type, "android-phone") == 0) {
            update_app_client_info(fd, peeraddress, peerport, qrcode, ONLINE, name, type);
            packet42 = packet42_create(qrcode, name, type);
            packet42_str = cJSON_Print(packet42);
            tmpstr = malloc(strlen(packet42_str) + 2);
            memset(tmpstr, 0, strlen(packet42_str) + 2);
            snprintf(tmpstr, strlen(packet42_str) + 2, "%s^", packet42_str);

            travel_app_list();
//            while (appt) {
//                char raddress[128];
//                bzero(raddress, 128);
//                int rport = get_peer_address(appt->socket_fd, raddress);
//                if (rport != appt->port || strcmp(appt->ip_addr_str, raddress) != 0) {
//                    appt = appt->next;
//                    continue;
//                }

                if (appt->current_state >= JOINED) {
                    //ret = write(appt->socket_fd,tmpstr,strlen(tmpstr));
                    int lport = get_local_sockinfo(appt->socket_fd);
                    if (lport > 0 && lport == IN_PORT) {
                        ret = write(appt->socket_fd, tmpstr, strlen(tmpstr));
                    } else
                        appt->current_state = OFFLINE;
                }
                if (ret == -1)
                    appt->current_state = OFFLINE;
//
//                appt = appt->next;
//            }

            cJSON_Delete(packet42);
            free(packet42_str);
            free(tmpstr);

            return cJSON_CreateString("family_join_waiting");

        } else {
            return cJSON_CreateString("family_join_deny");
        }

    }*/

}

//pakcet 3 process
cJSON *join_permit(jrpc_context *ctx, cJSON *params, cJSON *id) {
    cJSON *param = NULL;
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
    char *packet2 = "{\"result\":\"family_join_permit\"}";
    int ret = 0;

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    param = cJSON_GetObjectItem(params, "result");
    if (param != NULL && param->type == cJSON_String) {
        if (strcmp(param->valuestring, "permit") == 0) {
            param = cJSON_GetObjectItem(params, "qrcode");
            if (param != NULL && param->type == cJSON_String) {
//                while (appt) {
                    if (strcmp(appt->app_dev_id, param->valuestring) == 0) {
//                        ret = write(appt->socket_fd, packet2, strlen(packet2));
                        ret = send_app_data(packet2, strlen(packet2));
                        if (ret == -1)
                            appt->current_state = OFFLINE;
                        else
                            update_app_client_info(appt->socket_fd, NULL, 0, appt->app_dev_id, OFFLINE, appt->name,
                                                   appt->type);
//                        break;

                    }
//                    appt = appt->next;
//                }
            }

        }
    }
    return cJSON_CreateString("packet2_received");
}

int send_msg_and_recv(char *serveraddr, int port, char *cmd) {

    printf("=== (send_msg_and_recv). addr:%s,port:%d,time: %d\n", serveraddr,port,time(NULL)); //scott
//    return 1;

    int sockfd, n;
    char recvline[4096], sendline[4096];
    struct sockaddr_in servaddr;
    //char *cmd="{\"method\":\"opendoor_permit\"}\0";
    //if((strcmp(g_baseconf.innerinterfaceip,"127.0.0.1") == 0) ||(strlen(g_baseconf.innerinterfaceip)==0))
    if (eth_stat(g_baseconf.innerinterfacename) == 0)
        return 0;

//    puts("send_msg_and_recv == 1");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        DEBUG_ERR_LOG("create socket error: %s(errno: %d)", strerror(errno), errno);
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serveraddr, &servaddr.sin_addr) <= 0) {
        DEBUG_ERR_LOG("inet_pton error for %s", serveraddr);
        close(sockfd);
        return 0;
    }

//	puts("send_msg_and_recv == 2");
    unsigned long ul = 1;
//    ioctl(sockfd, FIONBIO, &ul);
    int ret = -1;
    fd_set set;
    struct timeval tm;
    int error = -1, len;
    len = sizeof(int);
    DEBUG_INFO("connect and send to %s %s", serveraddr, cmd);
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
//        tm.tv_sec = 2;
//        tm.tv_usec = 0;
//        FD_ZERO(&set);
//        FD_SET(sockfd, &set);
//        if (select(sockfd + 1, NULL, &set, NULL, &tm) > 0) {
//            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t * ) & len);
//            if (error == 0) ret = 0;
//        }
//    } else
//        ret = 0;
		close(sockfd);
		return 0;
	}
//	puts("send_msg_and_recv == 3");
//    if (ret == -1) {
//        close(sockfd);
//        DEBUG_ERR_LOG("connect error: %s(errno: %d)", strerror(errno), errno);
//        return 0;
//    }

//    ul = 0;
//    ioctl(sockfd, FIONBIO, &ul);

    //gets(sendline, 4096, stdin);
//	puts("send_msg_and_recv == 4");
//    puts(cmd);
//    printf("== len:%d\n",strlen(cmd));
	if (send(sockfd, cmd, strlen(cmd), 0) < 0) {
//        DEBUG_ERR_LOG("send msg error: %s(errno: %d)", strerror(errno), errno);
		puts("send_msg_and_recv == error");
        close(sockfd);
        return 0;
    }

//	puts("send_msg_and_recv == 5");

//        usleep(100000); // scott

//        ret = recv(sockfd,recvline,sizeof(recvline),0);
//        if(ret <= 0) {
//                close(sockfd);
//                return 0;
//        }
//        DEBUG_INFO("sockfd:%d %s %s",sockfd,cmd,recvline);
    //printf("sockfd:%d %s\n",sockfd,cmd);
    close(sockfd);
    printf("=== (send_msg_and_recv)...end time: %d\n", time(NULL)); //scott
    return 1;
}

int send_msg(char *serveraddr, char *cmd, int port) {
    int sockfd, n;
    char recvline[4096], sendline[4096];
    struct sockaddr_in servaddr;
    //char *cmd="{\"method\":\"opendoor_permit\"}\0";
    //if((strcmp(g_baseconf.innerinterfaceip,"127.0.0.1") == 0) ||(strlen(g_baseconf.innerinterfaceip)==0))
    if (eth_stat(g_baseconf.innerinterfacename) == 0)
        return 0;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("create socket error: %s(errno: %d)", strerror(errno), errno);
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serveraddr, &servaddr.sin_addr) <= 0) {
        printf("inet_pton error for %s", serveraddr);
        close(sockfd);
        return 0;
    }

//    unsigned long ul = 1;
//    ioctl(sockfd, FIONBIO, &ul);
    int ret = -1;
    fd_set set;
    struct timeval tm;
    int error = -1, len;
    len = sizeof(int);
    printf("connect and send to %s %s", serveraddr, cmd);
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
//        tm.tv_sec = 2;
//        tm.tv_usec = 0;
//        FD_ZERO(&set);
//        FD_SET(sockfd, &set);
//        if (select(sockfd + 1, NULL, &set, NULL, &tm) > 0) {
//            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t * ) & len);
//            if (error == 0) ret = 0;
//        }
//    } else
//        ret = 0;
		close(sockfd);
		puts(" connect server failed!\n");
		return 0;
	}

//    if (ret == -1) {
//        DEBUG_ERR_LOG("connect error: %s(errno: %d)", strerror(errno), errno);
//        close(sockfd);
//        return 0;
//    }

//    ul = 0;
//    ioctl(sockfd, FIONBIO, &ul);

    //gets(sendline, 4096, stdin);

    if (send(sockfd, cmd, strlen(cmd), 0) < 0) {
//        DEBUG_ERR_LOG("send msg error: %s(errno: %d)", strerror(errno), errno);
        close(sockfd);
        puts("send failed.");
        return 0;
    }
    printf("send succ,sockfd:%d %s", sockfd, cmd);
    close(sockfd);
    return 1;
}

void set_app_calling_and_send_accept_out(app_client_t *procer, char *acceptresult) {

    char *tmpstr = "{\"method\":\"call_ending\"}^";
    procer->current_state = CALLING;
    if (procer->line_state->calltype != OUTER2APP)
        write(procer->line_state->peersocketfd, acceptresult, strlen(acceptresult));
    int ret = 0;

//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
//    while (appt) {
    if(1){
        check_and_set_app_offline(appt);

        if (appt != procer && appt->current_state >= JOINED && appt->current_state < CALLING) {
            int lport = get_local_sockinfo(appt->socket_fd);
            if (lport > 0 && lport == IN_PORT) {
                ret = write(appt->socket_fd, tmpstr, strlen(tmpstr));
                if (ret <= 0)
                    update_app_line_state(appt, ZERO, 0, NULL, 0, OFFLINE);
                else
                    update_app_line_state(appt, ZERO, 0, NULL, 0, IDLE);
            } else
                update_app_line_state(appt, ZERO, 0, NULL, 0, OFFLINE);
        }

//        appt = appt->next;
    }

}

//when app get door open request,push 接听 按钮时发送消息给室内机
//室内机收到消息时，由该函数处理
cJSON *door_open_processing(jrpc_context *ctx, cJSON *params, cJSON *id) {
//    app_client_t *appt = g_app_client_list;
    app_client_t *procer;
    //char *tmpstr = "{\"door_open_state:\":\"processing\"}^";
    //char *tmpstr = "{\"method\":\"call_ending\"}^";
    char timestr[32];
    char event[512];

    memset(timestr, 0, 32);
    memset(event, 0, 512);

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    char cmd[1024];
    bzero(cmd, 1024);
    procer = get_appt_by_ctx(ctx);

//	snprintf(cmd,1024,"{\"method\":\"play_live_audio\",\"params\":{\"streamurl\":\"%s\"}}",procer->audiostream_out);
    snprintf(cmd, 1024, "{\"method\":\"play_live_audio\",\"params\":{\"streamurl\":\"%s\"}}",
             procer->audiostream_push_url_outer); // scott
    printf("== send cmd to outerbox : %s \n", cmd); // scott
    //state machine processing
    send_msg(procer->line_state->peeripaddr, cmd, OUTERBOX_PORT);
    set_app_calling_and_send_accept_out(procer, cmd);
    //log
    get_time_now(timestr);
    procer = get_appt_by_ctx(ctx);
    //snprintf(event,512,"接听(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
    snprintf(event, 512, "接听");
#ifdef LOG
    DEBUG_INFO("envent:%s", event);
#endif
    callhistory_log_to_db(timestr, g_last_caller, event, "单元机", procer->line_state->peeripaddr);

    return cJSON_CreateString("door_open_processing_has_done");
}

// tempory solutions for beijing chengyuan
// incomplete elevator guest cmd
#ifdef BEIJING_ELEVATOR
                                                                                                                        void * elevator_host_cmd_call() {

	pthread_detach(pthread_self());

	thread_sleep(40);

	GET_LOCAL_ADDRESS();
	char cmd[1024];
	bzero(cmd,1024);
	snprintf(cmd,1024,"{\"method\":\"call_elevator\",\"params\":{\"cmd\":2,\"ipaddr\":\"%s\"}}",g_baseconf.innerinterfaceip);
	send_msg(g_baseconf.outerboxip,cmd,OUTERBOX_PORT);

}
#endif

void *do_qrcode_opendoor_authcode_get_from_property() {
    char cmdstr[2048];
    bzero(cmdstr, 2048);

    pthread_detach(pthread_self());

    struct ev_loop *loop_init;
//        loop_init = ev_loop_new(0);
//        if (!loop_init) {
//                DEBUG_ERR_LOG("Can't initialise libev; bad $LIBEV_FLAGS in environment?");
//                exit(1);
//        }

    snprintf(cmdstr, 2048, "{\"method\":\"qrcode_opendoor_authcode_get\",\"params\":{\"ipaddr\":\"%s\"}}",
             g_baseconf.innerinterfaceip);

    //snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_otherinner\",\"params\":{\"doorid\":\"%s\"}}",g_callotherinner_doorid);
    DEBUG_INFO("cmdstr:%s", cmdstr);
    query_info_from_property(cmdstr, loop_init);
    //ev_run(loop_init, 0);
    DEBUG_INFO("%s", cmdstr);
}

cJSON *qrcode_opendoor_authcode_get(jrpc_context *ctx, cJSON *params, cJSON *id) {
    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    app_client_t *appt = get_appt_by_ctx(ctx);

    create_worker(do_qrcode_opendoor_authcode_get_from_property, NULL);
    if (appt)
        appt->app_state |= GETTING_QRCODE;

    return NULL;
}

cJSON *opendoor_permit(jrpc_context *ctx, cJSON *params, cJSON *id) {
    int ret = 0;
    char timestr[32];
    char event[512];
    app_client_t *procer;

    memset(timestr, 0, 32);
    memset(event, 0, 512);
    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    //do something
    char *cmd = "{\"method\":\"opendoor_permit\"}\0";
    procer = get_appt_by_ctx(ctx);
    ret = send_msg(procer->line_state->peeripaddr, cmd, OUTERBOX_PORT);
    //maybe outerbox is offline
    if (ret == 0) {
        g_outerbox_online = 0;
        return cJSON_CreateString("outerbox_is_offline");
    }

    GET_LOCAL_ADDRESS();
    //call_elevator
    char cmd_elevator[1024];
    bzero(cmd_elevator, 1024);
    snprintf(cmd_elevator, 1024, "{\"method\":\"call_elevator\",\"params\":{\"cmd\":1,\"ipaddr\":\"%s\"}}",
             g_baseconf.innerinterfaceip);
    send_msg(g_baseconf.outerboxip, cmd_elevator, OUTERBOX_PORT);

#ifdef BEIJING_ELEVATOR
    create_worker(elevator_host_cmd_call,NULL);
#endif
    g_outerbox_online = 1;
    //log
    get_time_now(timestr);
    //snprintf(event,512,"开门指令发出(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
    snprintf(event, 512, "开门指令发出");
#ifdef LOG
    DEBUG_INFO("envent:%s", event);
#endif
    callhistory_log_to_db(timestr, g_last_caller, event, "单元机", procer->line_state->peeripaddr);

    return cJSON_CreateString("opendoor_permit_has_done");

}

void generate_property_callcmdstr_to_app(app_client_t *appt, char *audiostr, char *cmdstr) {
    snprintf(cmdstr, 2048,
             "{\"method\":\"callin_from_property\",\"params\":{\"audio_from_property\":\"%s\",\"audio_to_property\":\"%s\"}}^",
             audiostr, appt->audiostream_url);
}

void generate_innerbox_callcmdstr_to_app(app_client_t *appt, char *params, char *cmdstr) {
    strncpy(cmdstr, params, 2048);
}

int select_idle_app_and_send_cmd(
        call_type_e calltype,
        char *peeraddress,
        int peerport,
        int peersocketfd,
        void *param
) {
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
    int ret = 0;
    int busy = 0;
    char cmdstr[2048];

//    while (appt) {

        if (appt->current_state >= JOINED && appt->current_state < CONNECTING) {
            int lport = get_local_sockinfo(appt->socket_fd);
            if (lport > 0 && lport == IN_PORT) {
                //snprintf(tmpstr,2048,"{\"method\":\"callin_from_property\",\"params\":{\"audio_from_property\":\"%s\",\"audio_to_property\":\"%s\"}}^",audio_str,appt->audiostream_url);
                switch (calltype) {
                    case PROPERTY2APP:
                        bzero(cmdstr, 2048);
                        generate_property_callcmdstr_to_app(appt, param, cmdstr);
                        break;
                    case OUTER2APP:
                        bzero(cmdstr, 2048);
                        //generate_opendoorcmd_to_app(appt,param1,cmdstr);
                        generate_outerbox_callcmdstr_to_app(appt, param, cmdstr);
                        break;
                    case APP2APP:
                        bzero(cmdstr, 2048);
                        generate_innerbox_callcmdstr_to_app(appt, param, cmdstr);
                        break;
                }
                printf("== write cmd to app: %s\n", cmdstr);
//                ret = write(appt->socket_fd, cmdstr, strlen(cmdstr));
                ret = send_app_data(cmdstr, strlen(cmdstr));

                if (ret > 0) {
                    update_app_line_state(appt, calltype, peerport, peeraddress, peersocketfd, CONNECTING);

                    busy++;
                } else
                    appt->current_state = OFFLINE;
            } else
                appt->current_state = OFFLINE;
        }

//        appt = appt->next;
//    }

    return busy;
}

cJSON *call_from_property(jrpc_context *ctx, cJSON *params, cJSON *id) {
    char *linebusy = "{\"result\":\"line_is_busy\"}";
    char tmpstr[2048];
    char audio_str[1024];
    bzero(audio_str, 1024);
    bzero(tmpstr, 2048);
    int ret = 0;
    int busy = 0;
    int outerport = 0;
    char peeraddress[128];
    bzero(peeraddress, 128);

    GET_LOCAL_ADDRESS();

    char timestr[128];
    bzero(timestr, 128);
    get_time_now(timestr);

    //get peeraddress from params
    //the peer may have multi addresses
    //so get_peer_address may get wrong address
    //outerport = get_peer_address(((struct jrpc_connection*)(ctx->data))->fd,peeraddress);

    if (g_nocallin_delay > 0 || app_is_busy() == 1 /*|| conf_is_ready(NULL)&SF == SF*/) {
        char name[64];
        bzero(name, 64);
        get_propertybox_name_byipaddr(peeraddress, name);
        callhistory_log_to_db(timestr, peeraddress, "呼入占线", name, peeraddress);
        return cJSON_CreateString(LINE_BUSY);
    }

    cJSON *param = cJSON_GetObjectItem(params, "audio");
    if (param != NULL && param->type == cJSON_String) {

#ifdef ALL_IN_ONE
        snprintf(audio_str,1024,"%s",param->valuestring);
#else
        char sip[64];
        bzero(sip, 64);
        GET_LOCAL_ADDRESS();
        url_replace(param->valuestring, audio_str, g_baseconf.innerinterfaceip, 1936);
        extract_ip_by_regex(param->valuestring, sip);
        int sport = extract_port_by_regex(param->valuestring);
        generate_iptables_rules(sip, sport, g_baseconf.innerinterfaceip, 1936);

#endif
    } else
        return cJSON_CreateString(WRONG_PARAMETER);

    //get peeraddress from params
    //the peer may have multi addresses
    //so get_peer_address may get wrong address
    param = cJSON_GetObjectItem(params, "ipaddr");
    if (param != NULL && param->type == cJSON_String) {
        strcpy(peeraddress, param->valuestring);
        outerport = PROPERTY_PORT;
    } else
        return cJSON_CreateString(WRONG_PARAMETER);

    //snprintf(tmpstr,2048,"{\"method\":\"callin_from_property\",\"params\":{\"audio_from_property\":\"%s\",\"audio_to_property\":\"%s\"}}^",audio_str,appt->audiostream_url);
    busy = select_idle_app_and_send_cmd(PROPERTY2APP, peeraddress, outerport,
                                        ((struct jrpc_connection *) (ctx->data))->fd, audio_str);
    //line is busy
    if (busy == 0)
        return cJSON_CreateString(LINE_BUSY);

    char name[64];
    bzero(name, 64);
    get_propertybox_name_byipaddr(peeraddress, name);
    callhistory_log_to_db(timestr, peeraddress, "呼入", name, peeraddress);

    return NULL;
}

cJSON *callaccept_from_property(jrpc_context *ctx, cJSON *params, cJSON *id) {
    char result[1024];
    bzero(result, 1024);
    char tmpstr[256];
    bzero(tmpstr, 256);
    snprintf(tmpstr, 256, "{\"method\":\"call_ending\"}^");
    int ret = 0;

    // send other CALLING APP 	call_ending
    //call

    //write back the result to property socket fd
    //
    app_client_t *procer = get_appt_by_ctx(ctx);
    snprintf(result, 1024, "{\"result\":\"call_accept\",\"audio\":\"%s\"}", procer->audiostream_out);
    DEBUG_INFO("g_stream_apptoout:%s %s", result, g_baseconf.outerboxip);
    set_app_calling_and_send_accept_out(procer, result);
    //write(callingapp->line_state->peersocketfd,result,strlen(result));

    //log
    char timestr[128];
    bzero(timestr, 128);
    char event[128];
    bzero(event, 128);
    get_time_now(timestr);
    strcpy(event, "本机接听");
    char name[64];
    bzero(name, 64);
    get_propertybox_name_byipaddr(procer->line_state->peeripaddr, name);
    callhistory_log_to_db(timestr, procer->ip_addr_str, event, name, procer->line_state->peeripaddr);

    return NULL;
}

cJSON *call_ending_of_otherinner(jrpc_context *ctx, cJSON *params, cJSON *id) {
    char *callending = "{\"method\":\"call_ending\"}^";
    int ret = 0;
//    return ;

//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
//    while (appt) {
        //if(appt->calling_otherinner>0 && appt->calling_otherinner != appt->socket_fd ) {
        //if(appt->line_state->calltype == APP2APP && appt->calling_otherinner>0 && appt->current_state>IDLE) {

        if (appt->line_state->calltype == APP2APP && appt->current_state > IDLE) {
            ret = write(appt->socket_fd, callending, strlen(callending));
            //appt->calling_otherinner = 0;
            bzero(appt->calling_otherinner_ip, 64);
            bzero(appt->calling_otherdoorid, 64);

            update_app_line_state(appt, ZERO, 0, NULL, 0, IDLE);

        }
        if (ret == -1)
            appt->current_state = OFFLINE;

//        appt = appt->next;
//    }
	// 注意， 返回值必须是 cJSON_String类型，或者 NULL
	return NULL;
}


cJSON *call_ending_from_app(jrpc_context *ctx, cJSON *params, cJSON *id) {
    DEBUG_INFO("in call_ending_from_app");

    char timestr[128];
    bzero(timestr, 128);
    char event[128];
    bzero(event, 128);
    get_time_now(timestr);
    char name[64];

    char tmpstr[1024];
    bzero(tmpstr, 1024);

    GET_LOCAL_ADDRESS();
    // scott 2018.12.9
//	snprintf(tmpstr,1024,"{\"method\":\"call_ending\",\"params\":{\"ipaddr\":\"%s\"}}",g_baseconf.outerinterfaceip);
    snprintf(tmpstr, 1024, "{\"method\":\"call_ending\",\"params\":{\"ipaddr\":\"%s\"}}", g_baseconf.innerinterfaceip);

    app_client_t *appt = get_appt_by_ctx(ctx);
    if (appt) {
        strcpy(event, "本机挂断");
        switch (appt->line_state->calltype) {
            case APP2PROPERTY:
                send_msg(appt->line_state->peeripaddr, tmpstr, PROPERTY_PORT);
                bzero(name, 64);
                get_propertybox_name_byipaddr(appt->line_state->peeripaddr, name);
                callhistory_log_to_db(timestr, name, event, name, appt->line_state->peeripaddr);
                break;
            case PROPERTY2APP:
                send_msg(appt->line_state->peeripaddr, tmpstr, PROPERTY_PORT);
                bzero(name, 64);
                get_propertybox_name_byipaddr(appt->line_state->peeripaddr, name);
                callhistory_log_to_db(timestr, name, event, name, appt->line_state->peeripaddr);
                break;
            case APP2APP:
                send_msg(appt->line_state->peeripaddr, "{\"method\":\"call_ending_of_otherinner\"}", OUT_PORT);
                callhistory_log_to_db(timestr, appt->line_state->peeripaddr, event, "住户", appt->line_state->peeripaddr);
                break;
            case OUTER2APP://to be move from
                //opendoor_deny procedure
                break;
            case APP2OUTER://to be implemented
                //only one direction
                break;
        }
        update_app_line_state(appt, ZERO, 0, NULL, 0, IDLE);
    }

    send_call_ending_to_connecting_app();

    //return NULL;
    return cJSON_CreateString("call_ending_has_done");
}

cJSON *call_ending_from_out(jrpc_context *ctx, cJSON *params, cJSON *id) {
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();

    char *callending = "{\"method\":\"call_ending\"}^";
    int ret = 0;
    char timestr[128];
    bzero(timestr, 128);
    char event[128];
    bzero(event, 128);
    get_time_now(timestr);
    char peeraddress[64];
    bzero(peeraddress, 64);
    char name[64];

    if (params) {
        cJSON *param = cJSON_GetObjectItem(params, "ipaddr");
        if (param && param->type == cJSON_String) {
            strcpy(peeraddress, param->valuestring);
        }
    }
    //may be no param ipaddr
    //get from peeraddress
    if (strlen(peeraddress) == 0) {
        get_peer_address(((struct jrpc_connection *) (ctx->data))->fd, peeraddress);
    }

//    appt = g_app_client_list;
//    while (appt) {
        if (appt->current_state >= CONNECTING && strcmp(appt->line_state->peeripaddr, peeraddress) == 0) {
//            ret = write(appt->socket_fd, callending, strlen(callending));
            ret = send_app_data( callending, strlen(callending));
            if (ret > 0)
                update_app_line_state(appt, ZERO, 0, NULL, 0, IDLE);
            else
                appt->current_state = OFFLINE;
        }

        strcpy(event, "对方挂断");
        switch (appt->line_state->calltype) {
            case APP2PROPERTY:
            case PROPERTY2APP:
                bzero(name, 64);
                get_propertybox_name_byipaddr(appt->line_state->peeripaddr, name);
                callhistory_log_to_db(timestr, appt->line_state->peeripaddr, event, "物业", appt->line_state->peeripaddr);
                break;
            case APP2OUTER:
                break;
            case OUTER2APP:
                callhistory_log_to_db(timestr, appt->line_state->peeripaddr, event, "单元机",
                                      appt->line_state->peeripaddr);
                break;
        }
//
//        appt = appt->next;
//    }

    return cJSON_CreateString("call_ending_has_done");
}

void send_call_ending_to_connecting_app() {
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
//    while (appt) {
        int ret = 0;
        char *cmdstr = "{\"method\":\"call_ending\"}^";
        if (appt->current_state == CONNECTING) {
            int lport = get_local_sockinfo(appt->socket_fd);
            if (lport > 0 && lport == IN_PORT) {
//                ret = write(appt->socket_fd, cmdstr, strlen(cmdstr));
                ret = send_app_data( cmdstr, strlen(cmdstr));
                update_app_line_state(appt, ZERO, 0, NULL, 0, IDLE);
                DEBUG_INFO("send door openprocessing: %s %s", appt->ip_addr_str, cmdstr);
            } else
                appt->current_state = OFFLINE;
        }

        if (ret == -1)
            update_app_line_state(appt, ZERO, 0, NULL, 0, OFFLINE);
//        appt = appt->next;
//    }

}

cJSON *opendoor_deny(jrpc_context *ctx, cJSON *params, cJSON *id) {
    int ret = 0;
    char timestr[32];
    char event[512];
    app_client_t *procer;

    memset(timestr, 0, 32);
    memset(event, 0, 512);

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    procer = get_appt_by_ctx(ctx);
    //state machine processing

    char *cmd = "{\"method\":\"call_ending\"}\0";

    ret = send_msg(procer->line_state->peeripaddr, cmd, OUTERBOX_PORT);

    update_app_client_info(((struct jrpc_connection *) (ctx->data))->fd, NULL, 0, NULL, IDLE, NULL, NULL);
    update_app_line_state(procer, ZERO, 0, NULL, 0, IDLE);
    //maybe outerbox is offline

    send_call_ending_to_connecting_app();
/*

        app_client_t * appt = g_app_client_list;
	while(appt) {
			int ret = 0;
			char *cmdstr = "{\"method\":\"call_ending\"}^";
			if(appt->current_state >= JOINED && appt->current_state < CALLING) {
				int lport = get_local_sockinfo(appt->socket_fd);
				if(lport>0 && lport == IN_PORT) {
					ret = write(appt->socket_fd,cmdstr,strlen(cmdstr));
					appt->current_state = IDLE;
					DEBUG_INFO("send door openprocessing: %s %s",appt->ip_addr_str,cmdstr);
				} else
					appt->current_state = OFFLINE;
			}

			if(ret == -1)
				update_app_client_info(appt->socket_fd, NULL, 0, NULL, OFFLINE, NULL, NULL);
		appt = appt->next;
	}
*/
    if (ret == 0) {
        g_outerbox_online = 0;
        return cJSON_CreateString("outerbox_is_offline");
    }
    g_outerbox_online = 1;

    //log
    get_time_now(timestr);
    //snprintf(event,512,"挂断(by %s )",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
    snprintf(event, 512, "挂断");
#ifdef LOG
    DEBUG_INFO("envent:%s", event);
#endif
//	callhistory_log_to_db(timestr,g_last_caller,event,"单元机",procer->line_state->peeripaddr);

    return cJSON_CreateString("opendoor_deny_has_done");
}

int get_page_of_callhistory() {
    char *sqlstr = "select count(*) from callhistory";

    return query_callhistory_count(sqlstr);
}

cJSON *callhistory_get(int page) {
    char *sqlstr = "select * from callhistory order by datetime(\"calltime\") desc";
    char sqlstr1[1024];
    memset(sqlstr1, 0, 1024);
    int pages = 0;
    int count = 0;

    snprintf(sqlstr1, 1024, "%s limit %d,%d", sqlstr, (page - 1) * PAGE_NUMBER, page * PAGE_NUMBER);
    count = get_page_of_callhistory();

    if (count <= PAGE_NUMBER)
        pages = 1;
    else
        pages = count / PAGE_NUMBER + ((count / PAGE_NUMBER * PAGE_NUMBER) == count ? 0 : 1);
    return query_callhistory_fromdb(sqlstr1, page, pages);
}

int get_page_of_msghistory() {
    char *sqlstr = "select count(*) from msghistory";

    return query_msghistory_count(sqlstr);
}

cJSON *msghistory_get(int page) {

    char *sqlstr = "select * from msghistory order by datetime(\"time\") desc";
    char sqlstr1[1024];
    memset(sqlstr1, 0, 1024);
    int pages = 0;
    int count = 0;

    snprintf(sqlstr1, 1024, "%s limit %d,%d", sqlstr, (page - 1) * PAGE_NUMBER, page * PAGE_NUMBER);
    count = get_page_of_msghistory();

    if (count <= PAGE_NUMBER)
        pages = 1;
    else
        pages = count / PAGE_NUMBER + ((count / PAGE_NUMBER * PAGE_NUMBER) == count ? 0 : 1);

    return query_msghistory_fromdb(sqlstr1, page, pages);

}

cJSON *callhistory_request(jrpc_context *ctx, cJSON *params, cJSON *id) {

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
    cJSON *page;
    int pgn = 0;

    if (params != NULL && params->type == cJSON_Object) {
        page = cJSON_GetObjectItem(params, "page");
        if (page != NULL && page->type == cJSON_Number) {
            pgn = page->valueint;
        }
    }

    cJSON *chj = callhistory_get(pgn);
    return chj;
}

cJSON *msghistory_request(jrpc_context *ctx, cJSON *params, cJSON *id) {

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
    cJSON *page;
    int pgn = 0;

    if (params != NULL && params->type == cJSON_Object) {
        page = cJSON_GetObjectItem(params, "page");
        if (page != NULL && page->type == cJSON_Number) {
            pgn = page->valueint;
        }
    }

    cJSON *mhj = msghistory_get(pgn);
    return mhj;
}

void *callin_check_func() {

    pthread_detach(pthread_self());

    int time = g_nocallin_delay * 3600;
    while (time > 0) {
        time = time - 10;
        thread_sleep(10);
    }
    g_nocallin_delay = 0;
}

cJSON *callin_set(jrpc_context *ctx, cJSON *params, cJSON *id) {
    //do call in set
    //2018.08.06
    cJSON *obj;

    if (params != NULL && params->type == cJSON_Object) {
        obj = cJSON_GetObjectItem(params, "denydelay");

        if (obj != NULL && obj->type == cJSON_Number) {

            g_nocallin_delay = obj->valueint;//hour
        }

    }
    create_worker(callin_check_func, NULL);

    return cJSON_CreateString("callinset_has_done");
}

void *do_callout_to_property(app_client_t *appt) {

    char portstr[16];
    bzero(portstr, 16);
    snprintf(portstr, 16, "%d", PROPERTY_PORT);
    char callingip[64];
    bzero(callingip, 64);

    pthread_detach(pthread_self());

    //app_client_t *appt = get_appt_by_ctx(ctx);
    if (strlen(appt->callout_ipaddr) == 0)
        strcpy(callingip, g_baseconf.callcenterip);
    else {
        strcpy(callingip, appt->callout_ipaddr);
        bzero(appt->callout_ipaddr, 64);
    }
    printf("=== (do_callout_to_property) try to connect : %s,%s\n", callingip, portstr); //scott

//	struct ev_loop *loopinthread =ev_loop_new(0);
//       	connect_watcher *c = new_connector(loopinthread,callingip,portstr);
//	if(!c) {
//		DEBUG_ERR_LOG("cannot connect to property:%s",callingip);
//		char tmpstr[512];
//		bzero(tmpstr,512);
//		snprintf(tmpstr,512,"{\"result\":\"call_error\",\"message\":\"无法连接到物业服务器：%s\"}",callingip);
//		write(appt->socket_fd,tmpstr,sizeof(tmpstr));
//		return NULL;
//
//		return cJSON_CreateString("cannot_connect_to_property");
//	}
//
//        c->buffer_size = 1500;
//        if(!c->buffer)
//                c->buffer = malloc(1500);
//        memset(c->buffer, 0, 1500);
//        c->pos = 0;

    //write command to property
    char call_from_innerbox[2048];
    GET_LOCAL_ADDRESS();
    //printf("appt$$$$$:%d %d\n",appt,ctx);
    //scott
//	snprintf(call_from_innerbox,2048,"{\"method\":\"call_from_innerbox\",\"params\":{\"audio\":\"%s\",\"ipaddr\":\"%s\"}}",appt->audiostream_out,g_baseconf.outerinterfaceip);
    snprintf(call_from_innerbox, 2048,
             "{\"method\":\"call_from_innerbox\",\"params\":{\"audio\":\"%s\",\"ipaddr\":\"%s\"}}",
             appt->audiostream_out, g_baseconf.innerinterfaceip);
//	c->data = call_from_innerbox;
//	c->eio.data = c;
    printf("call_from_innerbox:%s\n", call_from_innerbox);

//        if (c) {
    if (1) {
        printf(
                "Trying connection to %s:%d...", callingip, PROPERTY_PORT
        );
        //appt->calling_property = appt->socket_fd;
//		update_app_line_state(appt,APP2PROPERTY,PROPERTY_PORT,callingip,c->eio.fd,CONNECTING);
        update_app_line_state(appt, APP2PROPERTY, PROPERTY_PORT, callingip, 0, CONNECTING);
        send_recv_msg(callingip, PROPERTY_PORT, call_from_innerbox, strlen(call_from_innerbox));

//                ev_io_start(loopinthread, &c->eio);
//		ev_run(loopinthread,0);
    }
//        else {
//                DEBUG_ERR_LOG( "Can't create a new connection");
//                return cJSON_CreateString("Cannot_connect_to_Property");
//        }

    char timestr[128];
    bzero(timestr, 128);
    char event[128];
    bzero(event, 128);
    get_time_now(timestr);
    //app_client_t *procer = get_appt_by_ctx(ctx);
    strcpy(event, "呼出");
    callhistory_log_to_db(timestr, appt->ip_addr_str, event, "物业", callingip);

    return NULL;
}

cJSON *callout_to_property(jrpc_context *ctx, cJSON *params, cJSON *id) {

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
    if (app_is_busy())
        return cJSON_CreateString(LINE_BUSY);

    app_client_t *appt = get_appt_by_ctx(ctx);
    if (appt == NULL)
        return NULL;
    bzero(appt->callout_ipaddr, 64);
    DEBUG_INFO("********");
    cJSON *param = NULL;
    if (params != NULL)
        param = cJSON_GetObjectItem(params, "ipaddr");
    if (param != NULL && param->type == cJSON_String)
        snprintf(appt->callout_ipaddr, 64, "%s", param->valuestring);
    else
        snprintf(appt->callout_ipaddr, 64, "%s", g_baseconf.callcenterip);
    create_worker(do_callout_to_property, appt);
    return NULL;
}

cJSON *call_outerbox(jrpc_context *ctx, cJSON *params, cJSON *id) {

    //char urlstring[1024];
    int ret = 0;

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    cJSON *content = cJSON_CreateObject();
    cJSON *result = cJSON_CreateObject();
    cJSON *param = NULL;

    app_client_t *appt = get_appt_by_ctx(ctx);

    //sprintf(urlstring,"rtsp://%s:554/ch01_sub.h264",g_baseconf.innerinterfaceip);
    char streamurl[1024];
    bzero(streamurl, 1024);

    char audiostream_url[1024]; // scott
    bzero(audiostream_url, 1024);
    char randstr[10];

    memset(randstr, 0, 10);
    get_rand_str(randstr, 9);  //scott




/*
	if(strlen(g_baseconf.innerinterfaceip)==0)
		get_local_address();
*/
    GET_LOCAL_ADDRESS();
    //snprintf(streamurl,1024,"rtsp://admin:admin@%s:554/stream1",g_baseconf.outerboxip);
    if (params != NULL)
        param = cJSON_GetObjectItem(params, "name");
    // 地下室一层  scott
    if (param != NULL && param->type == cJSON_String && strcmp(param->valuestring, "basement1") == 0) {
        //负一楼室外机
#ifdef ALL_IN_ONE
                                                                                                                                //snprintf(streamurl,1024,"rtsp://admin:admin@%s:554/stream1",g_baseconf.outerboxip_1);
        	snprintf(streamurl,1024,g_baseconf.outerboxvideostreamurl_1,g_baseconf.outerboxip_1);
#else
        //snprintf(streamurl,1024,"rtsp://admin:admin@%s:556/stream1",g_baseconf.innerinterfaceip); //to be
        snprintf(streamurl, 1024, g_baseconf.outerboxvideostreamurl_1, g_baseconf.innerinterfaceip); //to be
#endif
        appt->line_state->calltype = APP2OUTER;
        update_app_line_state(appt, APP2OUTER, OUTERBOX_PORT, g_baseconf.outerboxip_1,
                              ((struct jrpc_connection *) (ctx->data))->fd, IDLE);

        generate_audio_stream(g_baseconf.outerboxip_1, audiostream_url, randstr);  //scott

    } else {

#ifdef ALL_IN_ONE
                                                                                                                                //snprintf(streamurl,1024,"rtsp://admin:admin@%s:554/stream1",g_baseconf.outerboxip);
        	snprintf(streamurl,1024,g_baseconf.outerboxvideostreamurl,g_baseconf.outerboxip);
#else
        //snprintf(streamurl,1024,"rtsp://admin:admin@%s:554/stream1",g_baseconf.innerinterfaceip);
        snprintf(streamurl, 1024, g_baseconf.outerboxvideostreamurl, g_baseconf.innerinterfaceip);
#endif
        appt->line_state->calltype = APP2OUTER;
        update_app_line_state(appt, APP2OUTER, OUTERBOX_PORT, g_baseconf.outerboxip,
                              ((struct jrpc_connection *) (ctx->data))->fd, IDLE);

        generate_audio_stream(g_baseconf.outerboxip, audiostream_url, randstr); //scott
    }

    DEBUG_INFO("call outerbox:%s", streamurl);
    cJSON_AddStringToObject(content, "video_from_outerbox", streamurl);
    //sprintf(urlstring,"rtmp://%s:1935/live",g_baseconf.innerinterfaceip);

//    cJSON_AddStringToObject(content,"audio_to_outerbox",appt->audiostream_url); //scott
    cJSON_AddStringToObject(content, "audio_to_outerbox", audiostream_url);  // scott

    cJSON_AddItemToObject(result, "call_outerbox_params", content);

    return result;
}


cJSON *call_elevator_inter(jrpc_context *ctx, cJSON *params, cJSON *id) {


    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
    char doorid[64];
    bzero(doorid, 64);

    cJSON *param = cJSON_GetObjectItem(params, "doorid");
    if (param != NULL && param->type == cJSON_String) {
        snprintf(doorid, 64, "%s", param->valuestring);
        DEBUG_INFO("doorid:%s", doorid);
    }

    innerbox_info_t *innerbox_t1 = select_from_innerbox_info_list_byid(doorid);
    if (innerbox_t1 == NULL)
        return cJSON_CreateString("call_elevator_inter_wrong_param");

    GET_LOCAL_ADDRESS();
    innerbox_info_t *innerbox_t2 = select_from_innerbox_info_list_byip(g_baseconf.innerinterfaceip);
    if (innerbox_t2 == NULL)
        return cJSON_CreateString("call_elevator_inter_wrong_param");
    if (strncmp(innerbox_t1->doorid, innerbox_t2->doorid, 6) != 0)
        return cJSON_CreateString("call_elevator_inter_not_same_element");

    char cmd[1024];
    bzero(cmd, 1024);
    snprintf(cmd, 1024,
             "{\"method\":\"call_elevator\",\"params\":{\"cmd\":3,\"ipaddr\":\"%s\",\"ipaddr_from\":\"%s\"}}",
             g_baseconf.innerinterfaceip, innerbox_t1->ipaddr);
    send_msg(g_baseconf.outerboxip, cmd, OUTERBOX_PORT);
    DEBUG_INFO("cmd:%s", cmd);
    return cJSON_CreateString("call_elevator_inter_has_done");
}

cJSON *call_elevator(jrpc_context *ctx, cJSON *params, cJSON *id) {

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
/*
	if(strlen(g_baseconf.innerinterfaceip)==0)
		get_local_address();
*/
    GET_LOCAL_ADDRESS();
    char cmd[1024];
    bzero(cmd, 1024);
    snprintf(cmd, 1024, "{\"method\":\"call_elevator\",\"params\":{\"cmd\":2,\"ipaddr\":\"%s\"}}",
             g_baseconf.innerinterfaceip);
    send_msg(g_baseconf.outerboxip, cmd, OUTERBOX_PORT);

    return cJSON_CreateString("call_elevator_has_done");
}

cJSON *call_accept_from_otherinner(jrpc_context *ctx, cJSON *params, cJSON *id) {
    char resultstr[256];
    bzero(resultstr, 256);
    strcpy(resultstr, "{\"result\":\"call_accept_from_otherinner\"}");

    // send other CALLING APP 	call_ending
    app_client_t *procer = get_appt_by_ctx(ctx);
    set_app_calling_and_send_accept_out(procer, resultstr);
    //call

    return cJSON_CreateString("call_accpet_from_otherinner_cmd_sent");
}

cJSON *call_from_otherinner(jrpc_context *ctx, cJSON *params, cJSON *id) {
    int busy = 0;
//    app_client_t *appt = g_app_client_list;
    int ret = 0;

    char tmpstr[2048];
    bzero(tmpstr, 2048);
    char *jstr = cJSON_Print(params);
//	snprintf(tmpstr,2048,"{\"method\":\"call_from_otherinner\",\"params\":{\"%s\"}}^",jstr);
    snprintf(tmpstr, 2048, "{\"method\":\"call_from_otherinner\",\"params\":%s}^", jstr);
    free(jstr);

    printf("method: call_from_otherinner.. %s\n", tmpstr);

    char raddress[128];
    bzero(raddress, 128);
    char otherinneraddr[128];
    bzero(otherinneraddr, 128);
    int fd = ((struct jrpc_connection *) (ctx->data))->fd;

    int otherport = get_peer_address(fd, otherinneraddr);

    char doorid[64];
    bzero(doorid, 64);
    cJSON *param = cJSON_GetObjectItem(params, "doorid");
    if (param != NULL && param->type == cJSON_String) {
        snprintf(doorid, 64, "%s", param->valuestring);
    }

    char timestr[64];
    bzero(timestr, 64);

    get_time_now(timestr);

    if (g_nocallin_delay > 0 || app_is_busy() == 1) {
        //log
        callhistory_log_to_db(timestr, doorid, "未接听", "住户", doorid);

        return cJSON_CreateString(LINE_BUSY);
    }

    busy = select_idle_app_and_send_cmd(APP2APP, otherinneraddr, otherport,
                                        ((struct jrpc_connection *) (ctx->data))->fd, tmpstr);
    if (busy == 0)
        return cJSON_CreateString("line_is_busy");
    printf("== busy code: %d \n", busy);

    callhistory_log_to_db(timestr, doorid, "呼入", "住户", doorid);

    return NULL;

}

void *do_otherinner_ipaddr_query_from_property() {
    char cmdstr[2048];
    bzero(cmdstr, 2048);

    printf("== do_otherinner_ipaddr_query_from_property start .. \n");

    struct ev_loop *loop_init;
//        loop_init = ev_loop_new(0);
//        if (!loop_init) {
//                DEBUG_ERR_LOG("Can't initialise libev; bad $LIBEV_FLAGS in environment?");
//                exit(1);
//        }

    snprintf(cmdstr, 2048, "{\"method\":\"ipaddr_query_of_innerbox\",\"params\":{\"ipaddr\":\"%s\"}}",
             g_baseconf.gateouterboxip);

    //snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_otherinner\",\"params\":{\"doorid\":\"%s\"}}",g_callotherinner_doorid);
    DEBUG_INFO("==== cmdstr:%s", cmdstr);
    query_info_from_property(cmdstr, loop_init);
    //ev_run(loop_init, 0);
    DEBUG_INFO("%s", cmdstr);

    printf("== func do_otherinner_ipaddr_query_from_property() reach end.\n");
}

void *otherinner_ipaddr_query_from_property() {

    pthread_detach(pthread_self());

    while (g_innerbox_get_flag == 0) {
        if (strlen(g_baseconf.gateouterboxip) == 0) {
            DEBUG_ERR_LOG("no gate outerbox ip");
            thread_sleep(3);
            continue;
        }

        thread_sleep(17);

        do_otherinner_ipaddr_query_from_property();
    }
}

cJSON *call_other_innerbox(jrpc_context *ctx, cJSON *params, cJSON *id) {

    char cmdstr[2048];
    bzero(cmdstr, 2048);

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
    if (app_is_busy())
        return cJSON_CreateString(LINE_BUSY);

    app_client_t *appt = get_appt_by_ctx(ctx);
    bzero(g_callotherinner_doorid, 32);
    cJSON *param = cJSON_GetObjectItem(params, "doorid");
    if (param != NULL && param->type == cJSON_String) {
        snprintf(g_callotherinner_doorid, 32, "%s", param->valuestring);
    } else
        return cJSON_CreateString("wrong_parameters_of_call_other_innerbox");

    innerbox_info_t *innerbox_t = select_from_innerbox_info_list_byid(g_callotherinner_doorid);
    DEBUG_INFO("*************doorid:%s", g_callotherinner_doorid);
    if (strcmp(g_baseconf.doorid, g_callotherinner_doorid) == 0) {
        char *busystr = "{\"result\":\"line_is_busy\"}^";
        write(appt->socket_fd, busystr, sizeof(busystr));
        return NULL;
    }

    if (innerbox_t == NULL) {

        char tmpstr[512];
        bzero(tmpstr, 512);
        snprintf(tmpstr, 512, "{\"result\":\"call_error\",\"message\":\"错误的门牌号码：%s\"}", g_callotherinner_doorid);
        write(appt->socket_fd, tmpstr, sizeof(tmpstr));
        return NULL;
        //return cJSON_CreateString("wrong_doorid_for_call");
    }

    //appt->calling_otherinner = appt->socket_fd;
    strcpy(appt->calling_otherdoorid, g_callotherinner_doorid);
    strcpy(appt->calling_otherinner_ip, innerbox_t->ipaddr);
    //just for test not immplement
    create_worker(do_call_other_innerbox, appt);
    return cJSON_CreateString("call_other_innerbox_request_sent");
}

void get_gpio_ncno() {
    int i = 0;
    for (i = 0; i < g_seczone_count; i++) {
        if (strcmp(g_seczone_conf[i].normalstate, "no") == 0) {
            g_seczone_conf[i].gpiolevel = 0;
        }
        if (strcmp(g_seczone_conf[i].normalstate, "nc") == 0) {
            g_seczone_conf[i].gpiolevel = 1;
        }
    }


}

void parse_secmode_to_seczone(int i) {
/*
	int i = 0;
	for(i=0;i<MODE_NUMBER;i++)

*/    {
        sscanf(g_seczone_mode_conf[i], "%[a-z]$%[a-z]$%[a-z]$%[a-z]$%[a-z]$%[a-z]$%[a-z]$%[a-z]",
               g_seczone_conf[0].onekeyset,
               g_seczone_conf[1].onekeyset,
               g_seczone_conf[2].onekeyset,
               g_seczone_conf[3].onekeyset,
               g_seczone_conf[4].onekeyset,
               g_seczone_conf[5].onekeyset,
               g_seczone_conf[6].onekeyset,
               g_seczone_conf[7].onekeyset);
    }
}

int get_seczone_conf() {
    dictionary *ini;

    int b = 0;
    int i = 0;
    double d;
    char *s;
    char zonename[64];
    memset(zonename, 0, 64);

    char modename[64];
    memset(modename, 0, 64);

    ini = iniparser_load(SECZONE_CONF);
    if (ini == NULL) {
        fprintf(stderr, "cannot parse file: %s\n", SECZONE_CONF);
        return -1;
    }

    s = iniparser_getstring(ini, "password:pass", NULL);
    if (s)
        strncpy(g_seczone_passwd, s, 512);


    g_seczone_count = iniparser_getint(ini, "zonenumber:count", 0);
    DEBUG_INFO("#####g_seczone_count:%d", g_seczone_count);

    g_onekey_set = iniparser_getint(ini, "onekeyset:state", 0);
    DEBUG_INFO("#####g_onekey_set:%d", g_onekey_set);

    g_seczone_mode = iniparser_getint(ini, "seczonemode:mode", 0);

    for (i = 0; i < MODE_NUMBER; i++) {

        sprintf(modename, "mode%d:value", i);
        s = iniparser_getstring(ini, modename, NULL);
        if (s)
            strncpy(g_seczone_mode_conf[i], s, 64);
    }

    g_onekey_set_func();

    for (i = 0; i < g_seczone_count; i++) {
        sprintf(zonename, "zone%d:port", i + 1);
        b = iniparser_getint(ini, zonename, 0);
        g_seczone_conf[i].port = b;

        sprintf(zonename, "zone%d:name", i + 1);
        s = iniparser_getstring(ini, zonename, NULL);
        if (s)
            strncpy(g_seczone_conf[i].name, s, 128);

        sprintf(zonename, "zone%d:normalstate", i + 1);
        s = iniparser_getstring(ini, zonename, NULL);
        if (s)
            strncpy(g_seczone_conf[i].normalstate, s, 16);

        sprintf(zonename, "zone%d:onekeyset", i + 1);
        s = iniparser_getstring(ini, zonename, NULL);
        if (s)
            strncpy(g_seczone_conf[i].onekeyset, s, 8);

        sprintf(zonename, "zone%d:currentstate", i + 1);
        s = iniparser_getstring(ini, zonename, NULL);
        if (s)
            strncpy(g_seczone_conf[i].currentstate, s, 8);

        sprintf(zonename, "zone%d:delaytime", i + 1);
        b = iniparser_getint(ini, zonename, 0);
        g_seczone_conf[i].delaytime = b;

        sprintf(zonename, "zone%d:nursetime", i + 1);
        b = iniparser_getint(ini, zonename, 0);
        g_seczone_conf[i].nursetime = b;

        sprintf(zonename, "zone%d:alltime", i + 1);
        s = iniparser_getstring(ini, zonename, NULL);
        if (s)
            strncpy(g_seczone_conf[i].alltime, s, 8);

        sprintf(zonename, "zone%d:gpiolevel", i + 1);
        if (s)
            g_seczone_conf[i].gpiolevel = iniparser_getint(ini, zonename, 0);

        sprintf(zonename, "zone%d:triggertype", i + 1);
        g_seczone_conf[i].triggertype = iniparser_getint(ini, zonename, 0);

        sprintf(zonename, "zone%d:online", i + 1);
        s = iniparser_getstring(ini, zonename, NULL);
        if (s)
            strncpy(g_seczone_conf[i].online, s, 8);
    }

    iniparser_freedict(ini);
    get_gpio_ncno();

    return 0;
}

void write_seczone_conf_file(void) {
    g_onekey_set_func();

    FILE *ini;
    int i = 0;
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
            "\n"
            "[seczonemode]\n"
            "mode = %d ;\n"
            "\n"
            "\n",
            g_seczone_passwd,
            g_onekey_set,
            g_seczone_mode);

    for (i = 0; i < MODE_NUMBER; i++) {
        fprintf(ini,
                "[mode%d]\n"
                "value = %s ;\n"
                "\n"
                "\n",
                i,
                g_seczone_mode_conf[i]);
    }

    fprintf(ini, "[zonenumber]\n"
                 "count = %d ;\n\n\n", g_seczone_count);

    for (i = 0; i < g_seczone_count; i++) {
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
                "gpiolevel = %d ;\n"
                "triggertype = %d ;\n"
                "online = %s ;\n"
                "\n"
                "\n",
                i + 1,
                g_seczone_conf[i].port,
                g_seczone_conf[i].name,
                g_seczone_conf[i].normalstate,
                g_seczone_conf[i].onekeyset,
                g_seczone_conf[i].currentstate,
                g_seczone_conf[i].delaytime,
                g_seczone_conf[i].nursetime,
                g_seczone_conf[i].alltime,
                g_seczone_conf[i].gpiolevel,
                g_seczone_conf[i].triggertype,
                g_seczone_conf[i].online
        );
    }
    fflush(ini);
    fclose(ini);
}

void *seczone_fileproc_thread() {
    int i = 0;
    int j = 0;
    while (1) {

        if (g_seczone_change == 1) {
            write_seczone_conf_file();
            j = 1;
            pthread_mutex_lock(&seczone_mutex);
            g_seczone_change = 0;
            pthread_mutex_unlock(&seczone_mutex);
        } else {
            //backup seczone conf per 60s
            //when file is changed
            i++;
            if ((i = 6) && (j == 1)) {
                backup_seczone_conf_file();
                i = 0;
                j = 0;
            }
        }

        thread_sleep(10);
    }
}

void seczonehistory_log_to_db(char *time, char *type, char *seczone, char *event) {
    char sqlstr[1024];
    memset(sqlstr, 0, 1024);

    snprintf(sqlstr, 1024, "insert into seczonehistory values ('%s', '%s', '%s', '%s')", time, type, seczone, event);
#ifdef SQL
    insert_data_to_db(sqlstr);
#endif
}

cJSON *seczone_mode_set(jrpc_context *ctx, cJSON *params, cJSON *id) {
    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    cJSON *mode = NULL;

    if (params != NULL && params->type == cJSON_Object) {
        mode = cJSON_GetObjectItem(params, "mode");
        if (mode != NULL && mode->type == cJSON_Number)
            g_seczone_mode = mode->valueint;

    }

    pthread_mutex_lock(&seczone_mutex);
    g_seczone_change = 1;
    pthread_mutex_unlock(&seczone_mutex);

    //write_seczone_conf_file();
    parse_secmode_to_seczone(g_seczone_mode);

    return NULL;
}

cJSON *seczone_mode_get(jrpc_context *ctx, cJSON *params, cJSON *id) {
    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    char mode[256];
    bzero(mode, 256);
    snprintf(mode, 256, "{\"result\":\"seczone_mode\",\"mode\":%d}^", g_seczone_mode);
    app_client_t *procer = get_appt_by_ctx(ctx);
    if (procer) {
        write(((struct jrpc_connection *) (ctx->data))->fd, mode, strlen(mode));
    }

    return NULL;
}

cJSON *seczone_modeconf_set(jrpc_context *ctx, cJSON *params, cJSON *id) {
    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    cJSON *mode = NULL;
    cJSON *value = NULL;

    if (params != NULL && params->type == cJSON_Object) {
        mode = cJSON_GetObjectItem(params, "mode");
        if (mode != NULL && mode->type == cJSON_Number) {

            value = cJSON_GetObjectItem(params, "value");
            if (value != NULL && value->type == cJSON_String)
                strcpy(g_seczone_mode_conf[mode->valueint], value->valuestring);
        }
    }
    //write to file
    pthread_mutex_lock(&seczone_mutex);
    g_seczone_change = 1;
    pthread_mutex_unlock(&seczone_mutex);

    //write_seczone_conf_file();
    parse_secmode_to_seczone(g_seczone_mode);

    return NULL;
}

cJSON *seczone_modeconf_get(jrpc_context *ctx, cJSON *params, cJSON *id) {
    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    cJSON *mode = NULL;
    char value[256];
    bzero(value, 256);

    if (params != NULL && params->type == cJSON_Object) {
        mode = cJSON_GetObjectItem(params, "mode");
        if (mode != NULL && mode->type == cJSON_Number) {
            snprintf(value, 256, "{\"result\":\"seczone_mode_conf\",\"mode\":\"%d\",\"value\":\"%s\"}^", mode->valueint,
                     g_seczone_mode_conf[mode->valueint]);
            app_client_t *procer = get_appt_by_ctx(ctx);
            if (procer)
                write(((struct jrpc_connection *) (ctx->data))->fd, value, strlen(value));
        }
    } else {

        snprintf(value, 256, "{\"result\":\"seczone_mode_conf\",\"value\":\"%s,%s,%s,%s\"}^", g_seczone_mode_conf[0],
                 g_seczone_mode_conf[1], g_seczone_mode_conf[2], g_seczone_mode_conf[3]);
        app_client_t *procer = get_appt_by_ctx(ctx);
        if (procer)
            write(((struct jrpc_connection *) (ctx->data))->fd, value, strlen(value));
    }

    return NULL;
}

cJSON *opendoor_pass_set(jrpc_context *ctx, cJSON *params, cJSON *id) {

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    //todo

    return cJSON_CreateString("opendoor_pass_set");
}

cJSON *antihijack_pass_set(jrpc_context *ctx, cJSON *params, cJSON *id) {
    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
    //to do

    return cJSON_CreateString("antihijack_pass_true");
}

cJSON *seczone_pass_set(jrpc_context *ctx, cJSON *params, cJSON *id) {
    //do sec zone password set
    //compare the old pass
    cJSON *oldpass, *newpass;
    app_client_t *procer;
    char event[512];
    char timestr[64];
    memset(event, 0, 512);
    memset(timestr, 0, 64);

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    if (params != NULL && params->type == cJSON_Object) {
        oldpass = cJSON_GetObjectItem(params, "oldpass");
        if (oldpass != NULL && oldpass->type == cJSON_String) {
            if (strcmp(g_seczone_passwd, oldpass->valuestring) == 0 || strlen(g_seczone_passwd) == 0) {
                newpass = cJSON_GetObjectItem(params, "newpass");
                if (newpass != NULL && newpass->type == cJSON_String)
                    strncpy(g_seczone_passwd, newpass->valuestring, 512);
                else
                    return cJSON_CreateString("seczone passwd must not be null!");
            } else
                return cJSON_CreateString("wrong old seczone passwd!");

        } else
            return cJSON_CreateString("wrong old seczone passwd!");

    }

    //log it
    get_time_now(timestr);
    procer = get_appt_by_ctx(ctx);
    //snprintf(event,512,"防区密码设置(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
    snprintf(event, 512, "防区密码设置");
    seczonehistory_log_to_db(timestr, "设置", "防区配置", event);

    pthread_mutex_lock(&seczone_mutex);
    g_seczone_change = 1;
    pthread_mutex_unlock(&seczone_mutex);
    //write_seczone_conf_file();

    return cJSON_CreateString("seczone_pass_set_has_done");
}

void init_seczone_conf_t() {
    int i = 0;

    for (i = 0; i < ZONE_NUMBER; i++) {
        g_seczone_conf[i].port = i + 1;
        memset(g_seczone_conf[i].name, 0, 128);
        memset(g_seczone_conf[i].normalstate, 0, 16);
        memset(g_seczone_conf[i].onekeyset, 0, 8);
        memset(g_seczone_conf[i].currentstate, 0, 8);
        g_seczone_conf[i].delaytime = 0;
        g_seczone_conf[i].delaycount = 0;
        memset(g_seczone_conf[i].alltime, 0, 8);
        memset(g_seczone_conf[i].online, 0, 8);
        g_seczone_conf[i].gpiolevel = 0;
        g_seczone_conf[i].triggertype = 0;
        g_seczone_conf[i].etime = 0;
        g_seczone_conf[i].nursetime = 0;
        g_seczone_conf[i].lastfailtime = 0;
    }
}

void travel_seczone_conf() {
    int i = 0;
    DEBUG_INFO("seczone pass:%s", g_seczone_passwd);
    for (i = 0; i < g_seczone_count; i++) {
        DEBUG_INFO("seczone:%d %s %s", g_seczone_conf[i].port, g_seczone_conf[i].name, g_seczone_conf[i].normalstate);
    }

}

cJSON *destroy_emergency(jrpc_context *ctx, cJSON *params, cJSON *id) {
    cJSON *password;
    GET_LOCAL_ADDRESS();
    if (params->type == cJSON_Object) {
        password = cJSON_GetObjectItem(params, "password");
        if (password != NULL && password->type == cJSON_String) {
            if (strcmp(password->valuestring, g_seczone_passwd) != 0)
                return cJSON_CreateString("destroy_emergency_wrong_password");
            else {
                char timestr[128];
                bzero(timestr, 128);
                get_time_now(timestr);
                char msgtoproperty[1024];
                bzero(msgtoproperty, 1024);
                // scott 2018.12.3
//				snprintf(msgtoproperty,1024,"{\"method\":\"destroy_emergency\",\"params\":{\"time\":\%s\",\"ipaddr\":\"%s\",\"seczone\":\"\"}}",timestr,g_baseconf.outerinterfaceip);
                snprintf(msgtoproperty, 1024,
                         "{\"method\":\"destroy_emergency\",\"params\":{\"time\":\"%s\",\"ipaddr\":\"%s\",\"seczone\":\"\"}}",
                         timestr, g_baseconf.innerinterfaceip);
                send_msg(g_baseconf.alarmcenterip, msgtoproperty, PROPERTY_PORT);

                return cJSON_CreateString("destroy_emergency_success");
            }
        } else return cJSON_CreateString("destroy_emergency_null_parameters");
    } else return cJSON_CreateString("destroy_emergency_wrong_parameters");

}

cJSON *verify_emergency_password(jrpc_context *ctx, cJSON *params, cJSON *id) {
    cJSON *password;
    if (params->type == cJSON_Object) {
        password = cJSON_GetObjectItem(params, "password");
        if (password != NULL && password->type == cJSON_String) {
            if (strcmp(password->valuestring, g_seczone_passwd) != 0)
                return cJSON_CreateString("verify_emergency_password_false");
            else {

                return cJSON_CreateString("verify_emergency_password_true");
            }
        } else return cJSON_CreateString("null_params");
    } else return cJSON_CreateString("wrong_parameters");

}

cJSON *seczone_conf_set(jrpc_context *ctx, cJSON *params, cJSON *id) {
    //do sec zone config set
    cJSON *password, *config, *item;
    cJSON *port, *name, *normalstate, *onekeyset, *currentstate, *delaytime, *nursetime, *alltime, *online;
    cJSON *triggertype, *gpiolevel;
    int arraysize = 0, i = 0;
    app_client_t *procer;
    char event[512];
    char timestr[64];
    memset(event, 0, 512);
    memset(timestr, 0, 64);

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    char *jsonprintstr = cJSON_Print(params);
    DEBUG_INFO("seczone_conf_set params:%s", jsonprintstr);
    free(jsonprintstr);

    if (params->type == cJSON_Object) {
        password = cJSON_GetObjectItem(params, "password");
        if (password != NULL && password->type == cJSON_String) {
            //do conf get
//			if(1) {
            DEBUG_INFO("g_seczone_passwd:%s", g_seczone_passwd);
            if (strcmp(password->valuestring, g_seczone_passwd) == 0) {
//			if(1) {
                config = cJSON_GetObjectItem(params, "config");
                if (config != NULL && config->type == cJSON_Array) {
                    arraysize = cJSON_GetArraySize(config);
                    while (i < arraysize) {

                        item = cJSON_GetArrayItem(config, i);
                        if (item != NULL && item->type == cJSON_Object) {
                            int k = 0;
                            port = cJSON_GetObjectItem(item, "port");
                            if (port != NULL && port->type == cJSON_Number) {
                                if (port->valueint <= ZONE_NUMBER && port->valueint > 0)
                                    k = port->valueint - 1;
                            }

                            g_seczone_conf[k].port = k + 1;

                            name = cJSON_GetObjectItem(item, "name");
                            if (name != NULL && name->type == cJSON_String)
                                strncpy(g_seczone_conf[k].name, name->valuestring, 128);
                            DEBUG_INFO("port->valueint:%d %d", port->valueint, i);
                            normalstate = cJSON_GetObjectItem(item, "normalstate");
                            if (normalstate != NULL && normalstate->type == cJSON_String)
                                strncpy(g_seczone_conf[k].normalstate, normalstate->valuestring, 16);

                            onekeyset = cJSON_GetObjectItem(item, "onekeyset");
                            if (onekeyset != NULL && onekeyset->type == cJSON_String)
                                strncpy(g_seczone_conf[k].onekeyset, onekeyset->valuestring, 8);

                            currentstate = cJSON_GetObjectItem(item, "currentstate");
                            if (currentstate != NULL && currentstate->type == cJSON_String)
                                strncpy(g_seczone_conf[k].currentstate, currentstate->valuestring, 8);

                            delaytime = cJSON_GetObjectItem(item, "delaytime");
                            if (delaytime != NULL && delaytime->type == cJSON_Number)
                                g_seczone_conf[k].delaytime = delaytime->valueint;

                            alltime = cJSON_GetObjectItem(item, "alltime");
                            if (alltime != NULL && alltime->type == cJSON_String)
                                strncpy(g_seczone_conf[k].alltime, alltime->valuestring, 8);

                            gpiolevel = cJSON_GetObjectItem(item, "gpiolevel");
                            if (gpiolevel != NULL && gpiolevel->type == cJSON_Number)
                                g_seczone_conf[k].gpiolevel = gpiolevel->valueint;

                            triggertype = cJSON_GetObjectItem(item, "triggertype");
                            if (triggertype != NULL && triggertype->type == cJSON_Number)
                                g_seczone_conf[k].triggertype = triggertype->valueint;

                            online = cJSON_GetObjectItem(item, "online");
                            if (online != NULL && online->type == cJSON_String)
                                strncpy(g_seczone_conf[k].online, online->valuestring, 8);

                            nursetime = cJSON_GetObjectItem(item, "nursetime");
                            if (nursetime != NULL && nursetime->type == cJSON_Number)
                                g_seczone_conf[k].nursetime = nursetime->valueint;
                        }
                        i++;

                    }
                }
            } else
                return cJSON_CreateString("wrong_seczone_password");
        } else
            return cJSON_CreateString("null_seczone_password");
    } else
        return cJSON_CreateString(WRONG_PARAMETER);

    get_gpio_ncno();

    if (params) {
        travel_seczone_conf();
        pthread_mutex_lock(&seczone_mutex);
        g_seczone_change = 1;
        pthread_mutex_unlock(&seczone_mutex);
        //write_seczone_conf_file();
    }

    //log it
    get_time_now(timestr);
    procer = get_appt_by_ctx(ctx);
    //snprintf(event,512,"防区参数设置(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
    snprintf(event, 512, "防区参数设置");
    seczonehistory_log_to_db(timestr, "设置", "防区配置", event);

    return cJSON_CreateString("seczone_conf_set_has_done");
}

//packet 16 create
cJSON *seczone_conf_p16_create() {
    int i = 0;
    cJSON *result_root = cJSON_CreateObject();
    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_obj = NULL;

    for (i = 0; i < g_seczone_count; i++) {

        content_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(content_obj, "port", g_seczone_conf[i].port);
        cJSON_AddStringToObject(content_obj, "name", g_seczone_conf[i].name);
        cJSON_AddStringToObject(content_obj, "normalstate", g_seczone_conf[i].normalstate);
        cJSON_AddNumberToObject(content_obj, "triggertype", g_seczone_conf[i].triggertype);
        cJSON_AddNumberToObject(content_obj, "gpiolevel", g_seczone_conf[i].gpiolevel);
        cJSON_AddStringToObject(content_obj, "onekeyset", g_seczone_conf[i].onekeyset);
        cJSON_AddStringToObject(content_obj, "currentstate", g_seczone_conf[i].currentstate);
        cJSON_AddNumberToObject(content_obj, "delaytime", g_seczone_conf[i].delaytime);
        cJSON_AddNumberToObject(content_obj, "nursetime", g_seczone_conf[i].nursetime);
        cJSON_AddStringToObject(content_obj, "online", g_seczone_conf[i].online);
        cJSON_AddStringToObject(content_obj, "alltime", g_seczone_conf[i].alltime);

        cJSON_AddItemToArray(content_array, content_obj);

        DEBUG_INFO("seczone: %d %d %d %s %s", ZONE_NUMBER, i, g_seczone_conf[i].port, g_seczone_conf[i].name,
                   g_seczone_conf[i].normalstate);

    }

    cJSON_AddItemToObject(result_root, "seczone_conf", content_array);

    return result_root;
}

cJSON *seczone_conf_require(jrpc_context *ctx, cJSON *params, cJSON *id) {

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
    //get seczone mode from params

    return seczone_conf_p16_create();
}

//save sec alert messages to file
//
void *secmsg_save(char *message) {

    pthread_detach(pthread_self());
    pthread_mutex_lock(&sec_mutex);
    FILE *fs = fopen(SECMSG_FILE, "a");

    if (fs != NULL) {
        fprintf(fs, "%s\n", message);
        fclose(fs);
    }
    pthread_mutex_unlock(&sec_mutex);

    system("sync");
}

//thread func for send old msg to property
void *send_old_secmsg() {
    pthread_detach(pthread_self());
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char *temp = NULL;
    int success = 0;
    char timestr[64];
    char lastmsg[256];
    char firstmsg[256];
    int count = 0;
    FILE *wstream = NULL;
    time_t now;

    while (1) {
        thread_sleep(10);
        //alarm center may be still down
        time(&now);
        if (now - g_secmsg_fail_time < 20)
            continue;

        success = 0;
        //SECMSG_FILE 's size not increase so wo try to process
        //first to remove duplicate lines
        pthread_mutex_lock(&sec_mutex);
        FILE *stream = fopen(SECMSG_FILE, "r");
        if (stream == NULL)
            goto retry;

        FILE *wstream = fopen(SECMSG_FILE_T, "w");
        if (wstream == NULL)
            goto retry;

        while ((read = getline(&line, &len, stream)) != -1) {
            temp = strtok(line, "\n");
            success = send_msg(g_baseconf.alarmcenterip, temp, PROPERTY_PORT);
            if (success != 1) {
                fprintf(wstream, "%s\n", temp);
            }
        }

        free(line);
        line = NULL;
        fclose(stream);
        fclose(wstream);

        char cmd[64];
        bzero(cmd, 64);
        snprintf(cmd, 64, "cp -f %s %s", SECMSG_FILE_T, SECMSG_FILE);
        system(cmd);
        system("sync");

        //all msg sent so we should
        //clear the SECMSG_FILE
        if (success == 1) {
            stream = fopen(SECMSG_FILE, "w");
            fclose(stream);
            get_time_now(timestr);
            seczonehistory_log_to_db(timestr, "报警", "#重发", "成功");
            system("sync");
        }
        retry:
        pthread_mutex_unlock(&sec_mutex);
    }
}

void send_alarm_to_property(int port, char *name, char *msg) {
    char timestr[64];
    char seczone[64];
    memset(timestr, 0, 64);
    memset(seczone, 0, 64);
    char msgtoproperty[1024];
    bzero(msgtoproperty, 1024);

    get_time_now(timestr);
    snprintf(seczone, 64, "防区:%d-%s-%s", port, name, msg);
/*
	if(strlen(g_baseconf.innerinterfaceip)==0)
		get_local_address();
*/
    GET_LOCAL_ADDRESS();
//	seczonehistory_log_to_db(timestr,"报警",seczone,message);

    //scott 2018.12.3
//	snprintf(msgtoproperty,1024,"{\"method\":\"emergency\",\"params\":{\"time\":\"%s\",\"ipaddr\":\"%s\",\"seczone\":\"%s\"}}",timestr,g_baseconf.outerinterfaceip,seczone);
    snprintf(msgtoproperty, 1024,
             "{\"method\":\"emergency\",\"params\":{\"time\":\"%s\",\"ipaddr\":\"%s\",\"seczone\":\"%s\"}}", timestr,
             g_baseconf.innerinterfaceip, seczone);
    printf("== send alarm to center, centerip:%s,port:%d,msg:%s\n",g_baseconf.alarmcenterip,PROPERTY_PORT,msgtoproperty);
    int ret = send_msg(g_baseconf.alarmcenterip, msgtoproperty, PROPERTY_PORT);
    //log to database
    if (ret == 1) {
        seczonehistory_log_to_db(timestr, "报警#已发送", seczone, msg);
    } else {/*we should record it and retry again*/
        seczonehistory_log_to_db(timestr, "报警#发送失败", seczone, msg);
        time_t now;
        time(&now);
        if (port != 999) {
            if (now - g_seczone_conf[port - 1].lastfailtime > 10)
                create_worker(secmsg_save, msgtoproperty);
            g_seczone_conf[port - 1].lastfailtime = now;
            g_secmsg_fail_time = now;
        } else
            create_worker(secmsg_save, msgtoproperty);
    }
}

cJSON *seczone_emergency(jrpc_context *ctx, cJSON *params, cJSON *id) {

//	char * msg = "{\"method\":\"seczone_emergency\",\"params\":{\"port\":1,\"name\":\"fire\",\"message\":\"xxxxx\"}}^";
    char msgtoproperty[1024];
    int ret = 0;
    cJSON *tmpjson;
    int port = 0;
    char name[64];
    char message[512];
    char timestr[64];
    char seczone[64];
    char msg[512];

    memset(name, 0, 64);
    memset(message, 0, 512);
    memset(msg, 0, 512);
    memset(timestr, 0, 64);
    memset(seczone, 0, 64);
    bzero(msgtoproperty, 1024);

    if (params != NULL)
        if (params->type == cJSON_Object) {
            tmpjson = cJSON_GetObjectItem(params, "port");
            if (tmpjson != NULL && tmpjson->type == cJSON_Number)
                port = tmpjson->valueint;
            tmpjson = cJSON_GetObjectItem(params, "name");
            if (tmpjson != NULL && tmpjson->type == cJSON_String)
                snprintf(name, 64, "%s", tmpjson->valuestring);
            tmpjson = cJSON_GetObjectItem(params, "message");
            if (tmpjson != NULL && tmpjson->type == cJSON_String)
                snprintf(message, 512, "%s", tmpjson->valuestring);
        }

    //未设防报警，记录日志
    if ((port > 0) && (port != 999)) {

        //if(strcmp(g_seczone_conf[port-1].currentstate,"on") != 0 && strcmp(g_seczone_conf[port-1].alltime,"yes") != 0 ){
        if ((g_onekey_set == 0 || strcmp(g_seczone_conf[port - 1].onekeyset, "off") == 0 ||
             strcmp(g_seczone_conf[port - 1].online, "yes") != 0) &&
            strcmp(g_seczone_conf[port - 1].alltime, "yes") != 0) {
            //log to database
            get_time_now(timestr);
            snprintf(seczone, 64, "防区:%d-%s", port, g_seczone_conf[port - 1].name);
            snprintf(msg, 512, "防区未设防/旁路，但传感器触发报警。(%s)", message);
            seczonehistory_log_to_db(timestr, "报警", seczone, msg);
            //return directly
            return cJSON_CreateString("seczone_emergency_notsend");
        }
    }


    //发送到物业报警处理中心
    send_alarm_to_property(port, name, message);

    //发送到APP
/*
	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root,"method","seczone_emergency");
	cJSON_AddItemToObject(root,"params",params);
	char * sendstr = cJSON_Print(root);
	char * tmpstr = malloc(strlen(sendstr)+2);
	memset(tmpstr,0,strlen(sendstr)+2);
	snprintf(tmpstr,strlen(sendstr)+2,"%s^",sendstr);
*/
    char msgtoapp[1024];
    bzero(msgtoapp, 1024);
    snprintf(msgtoapp, 1024,
             "{\"method\":\"seczone_emergency\",\"params\":{\"port\":%d,\"name\":\"%s\",\"message\":\"%s\"}}", port,
             name, message);
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
//    while (appt) {
        //if(appt->current_state >= JOINED) {
        if (appt->current_state >= ONLINE) {
//            ret = write(appt->socket_fd, msgtoapp, strlen(msgtoapp));
            ret = send_app_data( msgtoapp, strlen(msgtoapp));
            DEBUG_INFO("### emergency :%d %s", ret, msg);
        }
        if (ret == -1)
            appt->current_state = OFFLINE;

//        appt = appt->next;
//    }
/*
	free(tmpstr);
	free(sendstr);
	cJSON_Delete(root);
*/
    return cJSON_CreateString("seczone_emergency_received");
}

void g_onekey_set_func() {
    int i = 0;
    g_onekey_set = 0;

    for (i = 0; i < g_seczone_count; i++) {
        if (strcmp(g_seczone_conf[i].onekeyset, "on") == 0)
            g_onekey_set = 1;
    }

}

cJSON *seczone_onekey_set(jrpc_context *ctx, cJSON *params, cJSON *id) {

    //do some work
    app_client_t *procer;
    char event[512];
    char timestr[64];
    memset(event, 0, 512);
    memset(timestr, 0, 64);
    int i = 0;

    g_onekey_set = 1;
    for (i = 0; i < g_seczone_count; i++) {
        if (strcmp(g_seczone_conf[i].onekeyset, "on") == 0)
            strcpy(g_seczone_conf[i].currentstate, "on");
    }

    pthread_mutex_lock(&seczone_mutex);
    g_seczone_change = 1;
    pthread_mutex_unlock(&seczone_mutex);
    //write_seczone_conf_file();
    //backup_seczone_conf_file();

    //log it
    get_time_now(timestr);
    procer = get_appt_by_ctx(ctx);
    //snprintf(event,512,"一键设防(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
    snprintf(event, 512, "一键设防");
    seczonehistory_log_to_db(timestr, "设防", "所有防区", event);
    return cJSON_CreateString("seczone_onekey_set_has_done");
}

cJSON *seczone_onekey_reset(jrpc_context *ctx, cJSON *params, cJSON *id) {

    cJSON *password;
    //do some work
    int i = 0;

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);

    app_client_t *procer;
    char event[512];
    char timestr[64];
    memset(event, 0, 512);
    memset(timestr, 0, 64);

    if (params->type == cJSON_Object) {
        password = cJSON_GetObjectItem(params, "pass");
        if (password != NULL && password->type == cJSON_String) {
            DEBUG_INFO("g_seczone_passwd:%s %s", g_seczone_passwd, password->valuestring);
            if (strcmp(password->valuestring, g_seczone_passwd) == 0) {
                //do onekey reset
                g_onekey_set = 0;
                for (i = 0; i < g_seczone_count; i++) {
                    if (strcmp(g_seczone_conf[i].onekeyset, "on") == 0)
                        strcpy(g_seczone_conf[i].currentstate, "off");
                }

                pthread_mutex_lock(&seczone_mutex);
                g_seczone_change = 1;
                pthread_mutex_unlock(&seczone_mutex);
                //                        write_seczone_conf_file();
                //			backup_seczone_conf_file();
            } else
                return cJSON_CreateString("seczone_onekey_reset_wrong_password");

        } else
            return cJSON_CreateString("seczone_onekey_reset_wrong_password");
    } else
        return cJSON_CreateString("seczone_onekey_reset_wrong_password");

    //log it
    get_time_now(timestr);
    procer = get_appt_by_ctx(ctx);
    //snprintf(event,512,"一键撤防(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
    snprintf(event, 512, "一键撤防");
    seczonehistory_log_to_db(timestr, "撤防", "所有防区", event);

    return cJSON_CreateString("seczone_onekey_reset_has_done");
}

int get_page_of_seczonehistory() {
    char *sqlstr = "select count(*) from seczonehistory";

    return query_seczonehistory_count(sqlstr);
}

struct sec_record_struct {
    int page;
    app_client_t *appt;
};

cJSON *seczonehistory_get(int page) {
    char *sqlstr = "select * from seczonehistory order by datetime(\"time\") desc ";
    char sqlstr1[1024];
    memset(sqlstr1, 0, 1024);
    int pages = 0;
    int count = 0;
    //int page = ((struct sec_record_struct *)data)->page;
    char *jsonstr;
//	app_client_t * appt = ((struct sec_record_struct *)data)->appt;

    snprintf(sqlstr1, 1024, "%s limit %d,%d", sqlstr, (page - 1) * PAGE_NUMBER, page * PAGE_NUMBER);
    count = get_page_of_seczonehistory();

    if (count <= PAGE_NUMBER)
        pages = 1;
    else
        pages = count / PAGE_NUMBER + ((count / PAGE_NUMBER * PAGE_NUMBER) == count ? 0 : 1);
    return query_seczonehistory_fromdb(sqlstr1, page, pages);
}

cJSON *seczone_record_history(jrpc_context *ctx, cJSON *params, cJSON *id) {

    if (access_check(ctx) == 0)
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
    cJSON *page;
    int pgn = 0;
    struct sec_record_struct *data = malloc(sizeof(struct sec_record_struct));

    if (params != NULL && params->type == cJSON_Object) {
        page = cJSON_GetObjectItem(params, "page");
        if (page != NULL && page->type == cJSON_Number) {
            pgn = page->valueint;
        }
    }

    data->page = pgn;
    data->appt = get_appt_by_ctx(ctx);

//	create_worker(seczonehistory_get,(void*)data);
    return seczonehistory_get(pgn);
//	return NULL;
}

cJSON *app_update_request(jrpc_context *ctx, cJSON *params, cJSON *id) {
    char sqlstr[1024];
    int ret;
    cJSON *param, *devtype;
    memset(sqlstr, 0, 1024);

    //check the params
    if (params->type == cJSON_Object) {
        devtype = cJSON_GetObjectItem(params, "devtype");
        if (devtype != NULL && devtype->type == cJSON_String) {
            snprintf(sqlstr, 1024, "select * from updateinfo where devtype='%s'", devtype->valuestring);
        } else return cJSON_CreateString("app_update_request_wrong_param");
    }
    //read from database

#ifdef SQL
    cJSON *app = query_updateinfo_fromdb(sqlstr);
#endif

    if (app->child == 0) {
        cJSON_Delete(app);
        return cJSON_CreateString("app_update_no_new_version");
    }

    return app;
}

void check_outerbox_live2() {
    g_outerbox_online = send_msg_and_recv(g_baseconf.outerboxip, OUTERBOX_PORT, "{\"method\":\"show_version\"}");
}

cJSON *outerbox_netstat(jrpc_context *ctx, cJSON *params, cJSON *id) {
    app_client_t *procer = get_appt_by_ctx(ctx);
    int fd = ((struct jrpc_connection *) (ctx->data))->fd;
    if (access_check(ctx) == 0) {
        return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
    }

    check_outerbox_live2();

    if (g_outerbox_online == 0)
        return cJSON_CreateString("outerbox_is_offline");
    else
        return cJSON_CreateString("outerbox_is_online");
}

int check_alive(char *serveraddr, int port) {
    int sockfd, n;
    char recvline[4096], sendline[4096];
    struct sockaddr_in servaddr;
    //char *cmd="{\"method\":\"opendoor_permit\"}\0";

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        DEBUG_ERR_LOG("create socket error: %s(errno: %d)", strerror(errno), errno);
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serveraddr, &servaddr.sin_addr) <= 0) {
        DEBUG_ERR_LOG("inet_pton error for %s", serveraddr);
        close(sockfd);
        return 0;
    }

//    unsigned long ul = 1;
//    ioctl(sockfd, FIONBIO, &ul);
    int ret = -1;
    fd_set set;
    struct timeval tm;
    int error = -1, len;
    len = sizeof(int);
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
//        tm.tv_sec = 2;
//        tm.tv_usec = 0;
//        FD_ZERO(&set);
//        FD_SET(sockfd, &set);
//        if (select(sockfd + 1, NULL, &set, NULL, &tm) > 0) {
//            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t * ) & len);
//            if (error == 0) ret = 0;
//        }
//    } else
//        ret = 0;
		close(sockfd);
		return 0;
	}

//    if (ret == -1) {
//        close(sockfd);
//        DEBUG_ERR_LOG("connect error: %s(errno: %d)", strerror(errno), errno);
//        return 0;
//    }

    close(sockfd);
    return 1;

}


char *get_local_ip(char *address, size_t size, int count) ;

cJSON *propertyserver_netstat(jrpc_context *ctx, cJSON *params, cJSON *id) {
	printf("== propertyserver_netstat() , innerinterfaceip:%s\n",g_baseconf.innerinterfaceip);

	if(strlen(g_baseconf.innerinterfaceip) == 0){
		char address[40];
		get_local_ip(address, sizeof(address), 1); // scott
		strcpy(g_baseconf.innerinterfaceip, address);
		if( strlen(g_baseconf.innerinterfaceip)) {
			// 网络恢复强制重新加入family
			printf("== close app socket forcelly\n");
			close(get_app_client()->socket_fd);
			get_app_client()->socket_fd = -1;
			system("reboot");
		}
	}

    if (check_alive(g_baseconf.propertyip, 18699) == 1)
        return cJSON_CreateString("propertyserver_is_online");
    else
        return cJSON_CreateString("propertyserver_is_offline");

}

void msghistory_log_to_db(char *time, char *url) {
    char sqlstr[1024];
    memset(sqlstr, 0, 1024);

    snprintf(sqlstr, 1024, "insert into msghistory values ('%s','%s')", time, url);
#ifdef SQL
    insert_data_to_db(sqlstr);
#endif
}

//留言记录
//30秒
cJSON *video_record(jrpc_context *ctx, cJSON *params, cJSON *id) {

    char cmdstr[1024];
    memset(cmdstr, 0, 1024);
    time_t now;
    char rtspname[64];
    memset(rtspname, 0, 64);
    char timestr[64];
    memset(timestr, 0, 64);

    snprintf(rtspname, 64, "%d.mp4", (int) time(&now));
    //snprintf(cmdstr,1024,"ffmpeg -i %s -t 30 -vcodec copy %s%s &",g_stream_outerbox,MSGHISTORY_PATH,rtspname);
#ifdef LOG
    DEBUG_INFO("video_record:%s", cmdstr);
#endif
    system(cmdstr);

    get_time_now(timestr);
    msghistory_log_to_db(timestr, rtspname);
    return cJSON_CreateString("video_record_has_done");
}

cJSON *softupdate_query(jrpc_context *ctx, cJSON *params, cJSON *id) {

    char cmdstr[1024];
    GET_LOCAL_ADDRESS();
    snprintf(cmdstr, 1024, "{\"method\":\"softupdate_query\",\"params\":{\"version\":\"%s\",\"ipaddr\":\"%s\"}}",
             g_baseconf.version, g_baseconf.innerinterfaceip);
    send_msg(g_baseconf.propertyip, cmdstr, PROPERTY_PORT);

    return cJSON_CreateString(g_baseconf.version);
}

cJSON *ipconf_updated(jrpc_context *ctx, cJSON *params, cJSON *id) {
    create_worker(ipaddr_query_from_property, 0);

    return NULL;
}

cJSON *start_shell(jrpc_context *ctx, cJSON *params, cJSON *id) {
#ifdef ANDROID
                                                                                                                            //	system("busybox pkill -9 nc");
#ifdef YANGCHUANG
	system("busybox nc -l -p 1234 -e /system/bin/sh &");
#else
	system("/system/xbin/busybox nc -l -p 1234 -e sh &");
#endif
	return cJSON_CreateString("shell started connect to port 1234");
#else
#endif
    return NULL;
}

cJSON *show_version(jrpc_context *ctx, cJSON *params, cJSON *id) {

//    char version[128];
//    bzero(version, 128);
//    int count = 0;
//    app_client_t *appt = g_app_client_list;
//    app_client_t *appt = get_app_client();
//    while (appt) {
//        if (appt->current_state >= JOINED) {
//            if (strlen(appt->version) > 0) {
//                if (strlen(version) == 0) {
//                    strncpy(version, appt->version, 16);
//                    goto next;
//                }
//
//                if (strstr(version, appt->version) == NULL) {
//                    strcat(version, "+");
//                    strncat(version, appt->version, 16);
//                    if (count++ > 2)
//                        break;
//                }
//
//            }
//        }
//        next:
//        appt = appt->next;
//    }

    get_sys_version();
//    char version_a[1024];
//    bzero(version_a, 1024);
//    if (strlen(version) > 0) {
//        snprintf(version_a, 1024, "%s;APP:%s", g_baseconf.version, version);
/*
		if(g_conf_stat == 0)
			snprintf(version_a,256,"!!basic.conf文件损坏%sAPP:%s",g_baseconf.version,version);
*/
//        return cJSON_CreateString(version_a);
//    } else
        return cJSON_CreateString(g_baseconf.version);
}

cJSON *sys_state(jrpc_context *ctx, cJSON *params, cJSON *id) {
    char msg[1024];
    bzero(msg, 1024);
/*
	app_client_t *procer = get_appt_by_ctx(ctx);
	if(!procer) {
		snprintf(msg,1024,"{\"result\":\"sys_state\",\"message\":\"APP未加入家庭\"}^");
		write(((struct jrpc_connection *)(ctx->data))->fd,msg,strlen(msg));
		return NULL;
	}
*/
    int fd = ((struct jrpc_connection *) (ctx->data))->fd;

    char confmsg[1024];
    bzero(confmsg, 1024);
    strcpy(confmsg, "配置状态:");
    if (conf_is_ready(confmsg) == 0)
        strcat(confmsg, "正常");

    get_sys_version();
    if ((g_ipconf_get_flag != 1) || (g_innerbox_get_flag <= 0)) {
        snprintf(msg, 1024, "{\"result\":\"sys_state\",\"message\":\"内核版本:%s !!地址配置未获取;%s\"}^", g_baseconf.version,
                 confmsg);
    } else
        snprintf(msg, 1024, "{\"result\":\"sys_state\",\"message\":\"内核版本:%s 状态正常\"}^", g_baseconf.version);

    //write(procer->socket_fd,msg,strlen(msg));
    write(fd, msg, strlen(msg));
    return NULL;
}

cJSON *exit_server(jrpc_context *ctx, cJSON *params, cJSON *id) {
    jrpc_server_stop(&my_in_server);
    return cJSON_CreateString("Bye!");
}

#define MONTH_PER_YEAR   12   // 一年12月
#define YEAR_MONTH_DAY   20   // 年月日缓存大小
#define HOUR_MINUTES_SEC 20   // 时分秒缓存大小

int get_compile_time(char *compiletime) {
    const char year_month[MONTH_PER_YEAR][4] =
            {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char compile_date[YEAR_MONTH_DAY] = {0}, compile_time[HOUR_MINUTES_SEC] = {0}, i;
    char str_month[4] = {0};
    int year, month, day, hour, minutes, seconds;

    sprintf(compile_date, "%s", __DATE__); // "Aug 23 2016"
    sprintf(compile_time, "%s", __TIME__); // "10:59:19"

    sscanf(compile_date, "%s %d %d", str_month, &day, &year);
    sscanf(compile_time, "%d:%d:%d", &hour, &minutes, &seconds);

    for (i = 0; i < MONTH_PER_YEAR; ++i) {
        if (strncmp(str_month, year_month[i], 3) == 0) {
            month = i + 1;
            break;
        }
    }

    DEBUG_INFO("Compile time is = %d-%.2d-%.2d %.2d:%.2d:%.2d", year, month, day, hour, minutes, seconds);
    sprintf(compiletime, "%d%.2d%.2d", year, month, day);
    return hour;
}

void get_sys_version() {

    char compiletime[16];
    bzero(compiletime, 16);
    int hour = get_compile_time(compiletime);
    snprintf(g_baseconf.version, 64, VERSION, compiletime, hour);

}

void backup_extend_conf() {
    char cmd[256];
    bzero(cmd, 256);
    snprintf(cmd, 256, "cp %s %s", EXTEND_CONF, EXTEND_CONF_BAK);
    system(cmd);
    system("sync");
}

void recovery_extend_conf() {
    char cmd[256];
    bzero(cmd, 256);
    if (get_extendconf_fromini(EXTEND_CONF_BAK) == 0) {
        snprintf(cmd, 256, "cp %s %s", EXTEND_CONF_BAK, EXTEND_CONF);
        system(cmd);
        system("sync");
    } else {
    }
}

void backup_basic_conf() {
    char cmd[256];
    bzero(cmd, 256);
    snprintf(cmd, 256, "cp %s %s", BASIC_CONF, BASIC_CONF_BAK);
    system(cmd);
    system("sync");
}

void recovery_basic_conf() {
    char cmd[256];
    bzero(cmd, 256);
    if (get_baseconf_fromini(BASIC_CONF_BAK) == 0) {
        snprintf(cmd, 256, "cp %s %s", BASIC_CONF_BAK, BASIC_CONF);
        system(cmd);
        system("sync");
        g_conf_stat = 1;
    } else {
        g_conf_stat = 0;
    }
}

//read content from basic ini file
void get_basic_conf() {
    bzero(&g_baseconf, sizeof(g_baseconf));
    if (get_baseconf_fromini(BASIC_CONF) == 0) {
        backup_basic_conf();
    } else {
        recovery_basic_conf();
    }
}

void check_outerbox_live() {
    int s, len;
    struct sockaddr_in addr;
    int addr_len = sizeof(struct sockaddr_in);
    char buffer[1024];

    memset(buffer, 0, 1024);

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("udp socket");
        return;
    }

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(HEARTBEAT_PORT);
    addr.sin_addr.s_addr = inet_addr(g_baseconf.outerboxip);

    snprintf(buffer, 1024, "{\"method\":\"heartbeat\"}");

    set_nonblocking(s);

    sendto(s, buffer, strlen(buffer), 0, (struct sockaddr *) &addr, addr_len);

    bzero(buffer, 1024);
    usleep(1000);
    len = recvfrom(s, buffer, sizeof(buffer), 0, (struct sockaddr *) &addr, &addr_len);

    if (strcmp(buffer, "{\"method\":\"heartbeat\"}") == 0)
        g_outerbox_online = 1;
    else
        g_outerbox_online = 0;

    if (len > 0)
        g_outerbox_online = 1;

    DEBUG_INFO("receive :%s %d", buffer, len);
    close(s);
}

void udp_heartbeat_send(char *ipaddr, int port, char *content) {

    int s, len;
    struct sockaddr_in addr;
    int addr_len = sizeof(struct sockaddr_in);
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("udp socket");
        return;
    }

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ipaddr);

    sendto(s, content, strlen(content), 0, (struct sockaddr *) &addr, addr_len);

    close(s);
}

void heartbeat_cb(struct ev_loop *loop_, ev_periodic *w_, int revents) {

    DEBUG_INFO("heartbeat 30s ....");
    int s, len;
    struct sockaddr_in addr;
    int addr_len = sizeof(struct sockaddr_in);
    char buffer[1024];
    char appstate[16];

    memset(buffer, 0, 1024);
    bzero(appstate, 16);
/*
	if((s = socket(AF_INET,SOCK_DGRAM,0)) < 0){
		perror("udp socket");
		return;
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(HEARTBEAT_PORT);
	addr.sin_addr.s_addr = inet_addr(g_baseconf.propertyip);
*/
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
    strcpy(appstate, "offline");
//    while (appt != NULL) {
        if (appt->current_state >= JOINED) {
            strcpy(appstate, "online");
//            break;
        }
//        appt = appt->next;
//    }

    if (strlen(appstate) == 0)
        strcpy(appstate, "offline");
/*
	if(strlen(g_baseconf.innerinterfaceip) == 0)
		get_local_address();
*/
    GET_LOCAL_ADDRESS();
    snprintf(buffer, 1024, "{\"method\":\"heartbeat\",\"params\":{\"ipaddr\":\"%s\",\"appstate\":\"%s\"}}",
             g_baseconf.innerinterfaceip, appstate);
/*
	sendto(s,buffer,strlen(buffer),0,(struct sockaddr *) &addr, addr_len);

	close(s);
*/

    udp_heartbeat_send(g_baseconf.callcenterip, HEARTBEAT_PORT, buffer);
    udp_heartbeat_send(g_baseconf.alarmcenterip, HEARTBEAT_PORT, "1");

    check_outerbox_live2();
}

//using udp
void *heartbeat_to_property() {

    pthread_detach(pthread_self());
    while (1) {
        heartbeat_cb(NULL, NULL, 0);
        thread_sleep(300);
    }
//        struct ev_loop *loop_tick = ev_loop_new(0);
//        ev_periodic heartbeat_tick;
//        ev_periodic_init(&heartbeat_tick, heartbeat_cb, 0., 300.,0);
//        ev_periodic_start(loop_tick, &heartbeat_tick);
//	ev_run(loop_tick,0);

}

/*heatbeat client end*/
int send_recv_msg(const char *dest, unsigned short port, const char *msg, size_t size);


// 这个函数存在文件句柄泄露情况
void query_info_from_property(char *cmdstr, struct ev_loop *loop) {
    send_recv_msg(g_baseconf.propertyip, PROPERTY_PORT, cmdstr, strlen(cmdstr));
    return;

//    //char cmdstr[2048];
//    //close(g_connectout_watcher.eio.fd);
//    char portstr[16];
//    bzero(portstr, 16);
//    snprintf(portstr, 16, "%d", PROPERTY_PORT);
//    printf("=== (query_info_from_property) try to connect %s,%s\n", g_baseconf.propertyip, portstr); //scott
//
//    connect_watcher *c = new_connector2(loop, g_baseconf.propertyip, portstr);
//    if (!c) {
//        DEBUG_ERR_LOG("== (query_info_from_property) Cannot connect to property server. (%s,%s) \n",
//                      g_baseconf.propertyip, portstr);
////		ev_loop_destroy(loop); //scott
//        return;
//    }
//    //memset(&g_connectout_watcher,0,sizeof(struct connect_watcher));
//    c->buffer_size = 1500;
//    if (!c->buffer)
//        c->buffer = malloc(1500);
//    memset(c->buffer, 0, 1500);
//    c->pos = 0;
///*
//	bzero(cmdstr,2048);
//	snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_outerbox\",\"params\":{\"ipaddr\":\"%s\"}}",g_baseconf.innerinterfaceip);
//*/
//    //write command to property
//// 	send(c->eio.fd,cmdstr,strlen(cmdstr),0);
//    //set_nonblocking(c->eio.fd);
//    c->data = cmdstr;
//    c->eio.data = c;
//
//    if (c) {
//        DEBUG_INFO(
//                "== (query_info_from_property) Trying connection to %s:%d...", g_baseconf.propertyip, PROPERTY_PORT
//        );
//        ev_io_start(loop, &c->eio);
//        ev_run(loop, 0);
//    } else {
//        DEBUG_ERR_LOG("Can't create a new connection");
//        return;
//    }
//
//    printf(" == query_info_from_property() end.. \n");

}

static void create_worker(void *(*func)(), void *arg) {
    pthread_t thread;
    pthread_attr_t attr;
    int ret;

    pthread_attr_init(&attr);

    if ((ret = pthread_create(&thread, &attr, func, arg)) != 0) {
        fprintf(stderr, "Can't create thread: %s\n",
                strerror(ret));
        return;
    }
}

void *do_ipaddr_query_from_property(struct ev_loop *_loop_init) {
    char cmdstr[2048];
    bzero(cmdstr, 2048);
    //thread_sleep(30);
    struct ev_loop *loop_init;
//        loop_init = ev_loop_new(0);
//        if (!loop_init) {
//                DEBUG_ERR_LOG("Can't initialise libev; bad $LIBEV_FLAGS in environment?");
//                exit(1);
//        }

    printf("== (do_ipaddr_query_from_property) start.. time:%d\n", time(NULL));

    GET_LOCAL_ADDRESS(); // scott

//	snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_outerbox\",\"params\":{\"ipaddr\":\"%s\"}}",g_baseconf.outerinterfaceip);
    snprintf(cmdstr, 2048, "{\"method\":\"ipaddr_query_of_outerbox\",\"params\":{\"ipaddr\":\"%s\"}}",
             g_baseconf.innerinterfaceip); // scott 2018.12.6
    query_info_from_property(cmdstr, loop_init); // scott
    //ev_run(loop_init, 0);
    DEBUG_INFO("%s", cmdstr);
    printf("== do_ipaddr_query_from_property() -- end. time:%d\n", time(NULL));
}

void reset_app_state() {
//    app_client_t *appt = g_app_client_list;
    app_client_t *appt = get_app_client();
//    while (appt) {
        if (appt->socket_fd > 0)
            close(appt->socket_fd);

        appt->current_state = OFFLINE;
//        appt = appt->next;
//    }
}

void *ipaddr_query_from_property() {

    pthread_detach(pthread_self());

    struct ev_loop *loop_init;
//    loop_init = ev_loop_new(0); scott

    while (g_ipconf_get_flag == 0) {
        printf("==== *** (ipaddr_query_from_property) \n");
        thread_sleep(7);
        do_ipaddr_query_from_property(loop_init); // scott
//        return ;
    }
    //new ip conf get
    //reset app state
    reset_app_state();
}

void execute_cmd(const char *cmd, char *result) {
    char buf_ps[1024];
    char ps[1024] = {0};
    FILE *ptr;
    strcpy(ps, cmd);
    if ((ptr = popen(ps, "r")) != NULL) {
        while (fgets(buf_ps, 1024, ptr) != NULL) {
            strcat(result, buf_ps);
            if (strlen(result) > 1024)
                break;
        }
        pclose(ptr);
        ptr = NULL;
    } else {
        printf("popen %s error\n", ps);
    }
}


int send_cmd_to_local(char *serveraddr, char *cmd) {
    int sockfd, n;
    char recvline[4096], sendline[4096];
    struct sockaddr_in servaddr;
    //char *cmd="{\"method\":\"opendoor_permit\"}\0";

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        DEBUG_ERR_LOG("create socket error: %s(errno: %d)", strerror(errno), errno);
        return 0;
    }

    DEBUG_INFO("cmd:%s", cmd);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SEC_PORT);
    if (inet_pton(AF_INET, serveraddr, &servaddr.sin_addr) <= 0) {
        DEBUG_ERR_LOG("inet_pton error for %s", serveraddr);
        goto err;
    }

    int ret = -1;

    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        goto err;
    }

    if (send(sockfd, cmd, strlen(cmd), 0) < 0) {
        DEBUG_ERR_LOG("send msg error: %s(errno: %d)", strerror(errno), errno);
        goto err;
    }
    DEBUG_INFO("sockfd:%d %s", sockfd, cmd);
    usleep(100000);
    ret = recv(sockfd, recvline, sizeof(recvline), 0);
    if (ret <= 0) {
        goto err;
    }

    close(sockfd);
    return 1;
    err:
    close(sockfd);
    return 0;
}

int eth_stat(char *ifname) {
	return 1;


    struct ethtool_value edata;
    int fd = -1, err = 0;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, ifname);
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("Cannot get control socket");
        return 0;
    }

    err = ioctl(fd, SIOCGIFFLAGS, &ifr);
    if (err < 0) {
        printf("Maybe ethernet interface %s is not valid!", ifr.ifr_name);
        close(fd);
        return 0;
    }

    close(fd); // scott

    if (ifr.ifr_flags & IFF_RUNNING) {
        return 1;
    } else {
        return 0;
    }
}

void *server_watchdog() {

    pthread_detach(pthread_self());
    int pid = getpid();

    char cmd[256];
    bzero(cmd, 256);

#ifdef ANDROID
    sprintf(cmd,"busybox ls -l /proc/%d/fd|busybox wc -l",pid);
#else
    sprintf(cmd, "ls -l /proc/%d/fd|wc -l", pid);
#endif
    char result[1024];
    int stat = 0;

    while (1) {
        thread_sleep(60);

        travel_app_list();

        bzero(result, 1024);
        execute_cmd(cmd, result);
        printf("$$$$$$$$$$$$$:%d\n", atoi(result));
        if (atoi(result) > 1000 || send_cmd_to_local("127.0.0.1", "{\"method\":\"show_version\"}") == 0) {
            //if(atoi(result) > 1000 ) {
            DEBUG_ERR_LOG("service error & restart ");
            kill(pid, 9);
        }
        //watch eth0 state
        //if eth0 is down
        //just reboot the system
/*
		if(eth_stat("eth0")==0) {
			if(stat++ == 2)
				system("reboot");
		}else
			stat = 0;
*/
    }

}

#ifdef ANDROID

// app 不存在，自动启动app
//void *app_watchdog({
//
//	pthread_detach(pthread_self());
//
//	int count = 0;
//	while(1) {
//		int lport = 0;
//		int online = 0;
// 		app_client_t * appt = g_app_client_list;
//        	while(appt != NULL) {
//			lport = get_local_sockinfo(appt->socket_fd);
//			if(lport < 0 || lport != IN_PORT)
//				appt->current_state = OFFLINE;
//			if(appt->current_state >= ONLINE){
//				online++;
//				if(appt->current_state >= CALLING)
//					appt->state_count++;
//				else
//					appt->state_count = 0;
//
//				if(appt->state_count > 4) {
//					appt->state_count = 0;
//					appt->current_state = ONLINE;
//				}
//				count = 0;
//			}
//
//                	appt = appt->next;
//        	}
//
//		if(online == 0) {
//			count++;
//			if((count > 5) && (g_update_state == 0))
//				system("am start -n com.bc.keshiduijiang.pad/com.bc.keshiduijiang.pad.MainActivity");
//		}
//
//		thread_sleep(30);
//	}
//}

void send_emergency_per10s(int gpio_port);
/*heartbeat client start*/
void check_nurse_time()
{
	int i = 0,j=0;
	time_t now = 0;
	char cmdstr[1024];

	for(i=0;i<g_seczone_count;i++) {
		if(g_seczone_conf[i].triggertype == 3 && g_seczone_conf[i].nursetime > 0) {
			time(&now);
			if(g_last_nursetime == 0)
				continue;
			DEBUG_INFO("$$$$$$ nurse:%d %d",now,g_last_nursetime);
			if(now-g_last_nursetime > g_seczone_conf[i].nursetime) {
				send_emergency_per10s(i);
			}
		}
	}

}

//每10秒种报一次警
void send_emergency_per10s(int gpio_port)
{
	char cmdstr[1024];
	bzero(cmdstr,1024);
	time_t now;
	time(&now);
	char msg[64];
	bzero(msg,64);
	switch (g_seczone_conf[gpio_port].triggertype) {
		case 0:
			strncpy(msg,"瞬时报警",64);
			break;
		case 1:
			strncpy(msg,"延时报警",64);
			break;
		case 3:
			strncpy(msg,"看护报警",64);
			break;

	}

	if(now-g_seczone_conf[gpio_port].etime >= 10){
		snprintf(cmdstr,1024,"{\"method\":\"seczone_emergency\",\"params\":{\"port\":%d,\"name\":\"%s\",\"message\":\"%s\"}}",gpio_port+1,g_seczone_conf[gpio_port].name,msg);

		send_cmd_to_local("127.0.0.1",cmdstr);
		g_seczone_conf[gpio_port].etime = now;
	}
}

/*general gpio process*/
//@gpio_port port of gpio
void generate_emergency(int gpio_port)
{

	time_t now;
	time(&now);
	char message[64];
	bzero(message,64);

	switch (g_seczone_conf[gpio_port].triggertype) {
		//瞬时报警
		case 0:
		send_emergency_per10s(gpio_port);
		break;

		//延时报警
		case 1:
		g_seczone_conf[gpio_port].delaycount++;
		if(g_seczone_conf[gpio_port].delaycount >= g_seczone_conf[gpio_port].delaytime) {
			send_emergency_per10s(gpio_port);
		}
		break;

		//防劫持报警
		case 2:
		//only send to alarm center
		strncpy(message,"防劫持报警",64);
		send_alarm_to_property(gpio_port+1,g_seczone_conf[gpio_port].name,message);
		break;

		//看护报警
		case 3:
		//
		g_last_nursetime = now;
		break;

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
//			DEBUG_INFO("gpionum=%d,val=%d\n", i,arg[i].data); // scott
			//if(arg[i].data != Alarm_data[i]) {
			if(arg[i].data == g_seczone_conf[7-i].gpiolevel) {
				DEBUG_INFO("gpio %d is alarm\n",i);

				generate_emergency(7-i);

			} else {

				g_seczone_conf[7-i].delaycount = 0;
			}
		}
//#ifdef NURSE
		check_nurse_time();
//#endif
		thread_sleep(1);
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
            if(g_watchdog_stop == 1){
                break;  // 外部触发停止dog，引起主机重启  scott 2018.12.14
            }
		watchdog_kick();
		//正在升级，关闭看门狗
		if(g_update_state == 1) {
			watchdog_close();
			break;
		}
		thread_sleep(5);
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
		//正在升级，关闭看门狗
		if(g_update_state == 1) {
			StopWDog();
			break;
		}
                thread_sleep(5);
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
			if(ret == g_seczone_conf[i].gpiolevel) {

				generate_emergency(i);

			} else {

				g_seczone_conf[i].delaycount = 0;

			}
		}
//#ifdef NURSE
		check_nurse_time();
//#endif
		//usleep(500000);
		thread_sleep(1);
	}

}
#endif

#endif

unsigned long get_file_size(const char *path) {
    unsigned long filesize = -1;
    struct stat statbuff;
    if (stat(path, &statbuff) < 0) {
        return filesize;
    } else {
        filesize = statbuff.st_size;
    }
    return filesize;
}

void *clear_sqlite_by_datetime() {
    sqlite3 *db = 0;
    char *pErrMsg = 0;
    int ret = 0;
    char *sSQL1 = "delete from callhistory where calltime <= datetime('now','-90 day')";
    char *sSQL2 = "delete from msghistory where time <= datetime('now','-30 day')";
    char *sSQL3 = "delete from seczonehistory where time <= datetime('now','-90 day')";
    //连接数据库
    db = g_filedb;

    ret = sqlite3_exec(db, sSQL1, 0, 0, &pErrMsg);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "SQL create error: %s\n", pErrMsg);
        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！
    }

    ret = sqlite3_exec(db, sSQL2, 0, 0, &pErrMsg);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "SQL create error: %s\n", pErrMsg);
        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！
    }

    ret = sqlite3_exec(db, sSQL3, 0, 0, &pErrMsg);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "SQL create error: %s\n", pErrMsg);
        sqlite3_free(pErrMsg); //这个要的哦，要不然会内存泄露的哦！！！
    }

}

void remove_mp4() {
    char cmdstr[1024];
    memset(cmdstr, 0, 1024);
    snprintf("rm -rf %s*.mp4", 1024, MSGHISTORY_PATH);
    system(cmdstr);
}

void *check_innerdb() {
    pthread_detach(pthread_self());
    while (1) {
#ifdef ANDROID
        if(get_file_size(INNER_DB) >= 888*1024*1024)
#else
        if (get_file_size(INNER_DB) >= 64 * 1024 * 1024)
#endif
            clear_sqlite_by_datetime();
        if (get_file_size(DEBUG_LOG_FILE) >= 500 * 1024 * 1024) {
            char cmd[256];
            bzero(cmd, 256);
            snprintf(cmd, 256, "> %s", DEBUG_LOG_FILE);
            system(cmd);
        }

        thread_sleep(24 * 3600);
    }
}

void init_gseczone_mode_conf() {
    int i = 0;

    for (i = 0; i < MODE_NUMBER; i++) {
        bzero(g_seczone_mode_conf[i], 64);
    }
}

void write_pid_file() {
    int pidfile = open(PID_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (pidfile < 0) {
        DEBUG_ERR_LOG("cannot Open Pid file: %s", PID_PATH);

        exit(1);
    }

    struct flock fl;
    fl.l_whence = SEEK_CUR;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_type = F_WRLCK;
    //if(lockf(pidfile,F_TLOCK,0) < 0) {
    //if(flock(pidfile,LOCK_EX|LOCK_NB) < 0) {
    if (fcntl(pidfile, F_SETLK, &fl) < 0) {
        exit(0);
    }

    char pid[32] = {0};
    snprintf(pid, 32, "%d\n", getpid());
    ssize_t len = strlen(pid);
    ssize_t ret = write(pidfile, pid, len);
    if (ret != len) {

        DEBUG_ERR_LOG("cannot Write Pid file: %s", PID_PATH);
        exit(0);
    }
}

/*
 * 程序获取ip的动作快于系统网卡初始化时间，所以增加等待时间。
 *
 */
char *get_local_ip(char *address, size_t size, int count) {
    char *cmd = "/system/bin/busybox ip addr | /system/bin/busybox grep inet | /system/bin/busybox grep -v 127 | /system/bin/busybox grep -v inet6 | busybox awk '{print $2}'| busybox awk -F '/' '{print $1}' > /data/user/local.txt";
    int i = 0;
    memset(address, 0, size);
    for (i = 0; i < count; i++) {
        printf("== Try LocalIP Detecting.. %d \n", i + 1);
        int ret = system(cmd);

        FILE *fp = fopen("/data/user/local.txt", "r");
//    FILE * fp = fopen("/system/xbin/local.txt","r");
        if (!fp) {
            sleep(1);
            continue;
        }
        if (fp) {
            fread(address, 1, size, fp);
            if (strlen(address) == 0) {
                sleep(1);
                fclose(fp);
                continue;
            }
            printf("== Read local address: %s\n", address);
            fclose(fp);
            break;
        }
    }

    if (strlen(address)) {
        char last_char = address[strlen(address) - 1];
        if ((last_char >= '0' && last_char <= '9') || last_char == '.') {

        } else {
            address[strlen(address) - 1] = '\0';
        }
    }
    return address;
}


// 2018.12.14
char *system_stat();

cJSON *system_query_stat(jrpc_context *ctx, cJSON *params, cJSON *id) {
    char *info = system_stat();
    int fd = ((struct jrpc_connection *) (ctx->data))->fd;
    if (info) {
        write(fd, info, strlen(info));
        thread_sleep(1);
        close(fd);
        free(info);

    }
    return NULL;
}


cJSON *system_reboot(jrpc_context *ctx, cJSON *params, cJSON *id) {
    g_watchdog_stop = 1;
    return NULL;
}

//定时检查当前连接是否忙碌，超过2分钟强制设置为IDLE
void * call_status_check(){
    time_t  start= 0;
    int MAX_BUSY_TIME = 60*2;
    printf("== start call_status_check() \n");
    while(1) {
        thread_sleep(1);
        if(start == 0) {
            if (get_app_client()->current_state >= CONNECTING) {
                start = time(NULL);
            }
        }else{
            if(get_app_client()->current_state < CONNECTING){
                start = 0;
                continue;
            }
            if( time(NULL) - start > MAX_BUSY_TIME ){
                get_app_client()->current_state = IDLE;
                start = 0;
                printf("=== busy time reached limit number(120s), reset to IDLE\n");
            }
        }

    }
}

int main(void) {
    char cmdstr[2048];
    int i = 0;

    char address[40];
    get_local_ip(address, sizeof(address), 30); // scott


	app_client_init();
//	return 0;

    write_pid_file();

    //signal(SIGPIPE,SIG_IGN);
    //signal(SIGCHLD,SIG_IGN);

    init_gseczone_mode_conf();

    memset(g_seczone_passwd, 0, 512);
    memset(g_baseconf.innerinterfaceip, 0, 64);
    memset(g_localnetmask, 0, 64);
    bzero(cmdstr, 2048);
    bzero(g_call_from_innerbox, 2048);
    for (i = 0; i < SECMSG_MAX; i++)
        bzero(g_secmsg_to_send[i], 512);

    init_seczone_conf_t();

    get_basic_conf();
    get_extendconf_fromini(EXTEND_CONF);
    if (get_seczone_conf() != 0) {
        recovery_seczone_conf_file();
        get_seczone_conf();
    } else
        backup_seczone_conf_file();

    travel_seczone_conf();
    //strcpy(g_last_callin_address,g_baseconf.outerboxip);
    get_sys_version();

    //wait 15s
    //thread_sleep(15);
    GET_LOCAL_ADDRESS();

    strcpy(g_baseconf.innerinterfaceip, address); // scott 2018.11.27

    //try to reconfig net interface
    //if(strlen(g_baseconf.innerinterfaceip) == 0)
    do_set_ip();
    //create_basic_conf_file();
    //get basic conf from ini file

    if (read_innerbox_info_from_conf() != 0) {
        recovery_innerbox_conf();
        read_innerbox_info_from_conf();
    } else
        backup_innerbox_conf();
    //travel_innerbox_infolist();




#ifdef SQL
                                                                                                                            create_sqlite_table();
	//db_insert_test();
	//attach_db();
#endif
    //insert_appinfo_to_db("insert into appinfo values('123456','ddd','2017-03-02 10:14:06')");
    //query_appinfo_fromdb();
    DEBUG_INFO("version:%s", g_baseconf.version);


    create_worker(ipaddr_query_from_property, 0); // scott  启动线程根据室外机地址查询室外机相关信息，包括地下一层

    create_worker(heartbeat_to_property, 0);
    create_worker(otherinner_ipaddr_query_from_property, 0); //scott
    create_worker(call_status_check, 0); //scott 2018.12.18



//#ifdef ANDROID


//#ifdef YANGCHUANG
//	for(i=0;i<g_seczone_count;i++) {
//		SetIoMode(i,IO_INTR_MODE,IO_INTR_BOTH_EDGE_TRIGGERED);
//	}
//	create_worker(get_gpio_status,NULL);
//#endif
//
//#ifdef YANGCHUANG
//        create_worker(watchdog_feeder,NULL);
//#endif


#ifdef MAICHONG
	system("/system/xbin/echo_test &");  //
	create_worker(watchdog_feeder,NULL); // scott
	create_worker(get_gpio_status,NULL);
#endif

#ifdef _APP_WATCHDOG
//        create_worker(app_watchdog,NULL); // scott
#endif

//#endif

    create_worker(send_old_secmsg, NULL);

//        create_worker(server_watchdog,NULL);

    create_worker(check_innerdb, NULL);

    create_worker(seczone_fileproc_thread, NULL);

//	jrpc_server_init(&my_in_server, IN_PORT);
//	jrpc_server_init(&my_out_server, OUT_PORT);
//	jrpc_server_init(&my_sec_server, SEC_PORT);

    server_init(NULL, IN_PORT);
    puts("== server_init() 1\n");


    server_init(&my_in_server, IN_PORT);
    puts("== server_init() 2\n");
    server_init(&my_out_server, OUT_PORT);
    puts("== server_init() 3\n");
    server_init(&my_sec_server, SEC_PORT);


    puts("== register procedure() start\n");
    jrpc_register_procedure(&my_in_server, init_setup, "init_setup", NULL);
    jrpc_register_procedure(&my_in_server, config_reset, "config_reset", NULL);
    jrpc_register_procedure(&my_in_server, ipconf_get, "ipconf_get", NULL);
    jrpc_register_procedure(&my_in_server, say_hello, "sayHello", NULL);
    jrpc_register_procedure(&my_in_server, callin_set, "callinset", NULL);
    jrpc_register_procedure(&my_in_server, callout_to_property, "callout_to_property", NULL);
    jrpc_register_procedure(&my_in_server, callaccept_from_property, "callaccept_from_property", NULL);
    jrpc_register_procedure(&my_in_server, call_outerbox, "call_outerbox", NULL);
    jrpc_register_procedure(&my_in_server, call_elevator, "call_elevator", NULL);
    jrpc_register_procedure(&my_in_server, call_elevator_inter, "call_elevator_inter", NULL);
    jrpc_register_procedure(&my_in_server, call_other_innerbox, "call_other_innerbox", NULL);
    jrpc_register_procedure(&my_in_server, call_accept_from_otherinner, "call_accept_from_otherinner", NULL);
    jrpc_register_procedure(&my_in_server, seczone_pass_set, "seczone_pass_set", NULL);
    jrpc_register_procedure(&my_in_server, seczone_conf_set, "seczone_conf_set", NULL);
    jrpc_register_procedure(&my_in_server, seczone_mode_set, "seczone_mode_set", NULL);
    jrpc_register_procedure(&my_in_server, seczone_mode_get, "seczone_mode_get", NULL);
    jrpc_register_procedure(&my_in_server, seczone_modeconf_set, "seczone_modeconf_set", NULL);
    jrpc_register_procedure(&my_in_server, seczone_modeconf_get, "seczone_modeconf_get", NULL);
    jrpc_register_procedure(&my_in_server, seczone_conf_require, "seczone_conf_require", NULL);
    jrpc_register_procedure(&my_in_server, seczone_onekey_set, "seczone_onekey_set", NULL);
    jrpc_register_procedure(&my_in_server, seczone_onekey_reset, "seczone_onekey_reset", NULL);
    jrpc_register_procedure(&my_in_server, seczone_record_history, "seczone_record_history", NULL);
    jrpc_register_procedure(&my_in_server, antihijack_pass_set, "antihijack_pass_set", NULL);
    jrpc_register_procedure(&my_in_server, destroy_emergency, "destroy_emergency", NULL);
    jrpc_register_procedure(&my_in_server, verify_emergency_password, "verify_emergency_password", NULL);
    jrpc_register_procedure(&my_in_server, join_family, "option_joinfamily", NULL);
    jrpc_register_procedure(&my_in_server, join_permit, "join_permit", NULL);
    jrpc_register_procedure(&my_in_server, door_open_processing, "door_open_processing", NULL);
    jrpc_register_procedure(&my_in_server, qrcode_opendoor_authcode_get, "qrcode_opendoor_authcode_get", NULL);
    jrpc_register_procedure(&my_in_server, opendoor_permit, "opendoor_permit", NULL);
    jrpc_register_procedure(&my_in_server, opendoor_deny, "opendoor_deny", NULL);
    jrpc_register_procedure(&my_in_server, opendoor_pass_set, "set_opendoor_pass", NULL);
    jrpc_register_procedure(&my_in_server, call_ending_from_app, "call_ending", NULL);
    jrpc_register_procedure(&my_in_server, call_ending_from_app, "call_ending_of_timeout", NULL);
    jrpc_register_procedure(&my_in_server, msghistory_request, "messagehistory_request", NULL);
    jrpc_register_procedure(&my_in_server, callhistory_request, "callhistory_request", NULL);
    jrpc_register_procedure(&my_in_server, app_update_request, "app_update_request", NULL);
    jrpc_register_procedure(&my_in_server, outerbox_netstat, "outerbox_netstat", NULL); //scott
    jrpc_register_procedure(&my_in_server, propertyserver_netstat, "propertyserver_netstat", NULL); // scott
    jrpc_register_procedure(&my_in_server, exit_server, "exit", NULL);
    jrpc_register_procedure(&my_in_server, seczone_emergency, "seczone_emergency", NULL);
    jrpc_register_procedure(&my_in_server, show_version, "show_version", NULL);
    jrpc_register_procedure(&my_in_server, sys_state, "sys_state", NULL);
    jrpc_register_procedure(&my_out_server, sys_state, "sys_state", NULL);
    jrpc_register_procedure(&my_out_server, show_version, "show_version", NULL);
    jrpc_register_procedure(&my_out_server, open_door, "opendoor", NULL);
    jrpc_register_procedure(&my_out_server, call_from_property, "call_from_property", NULL);
    jrpc_register_procedure(&my_out_server, call_from_otherinner, "call_from_otherinner", NULL);
    jrpc_register_procedure(&my_out_server, call_ending_from_out, "call_ending", NULL);
    jrpc_register_procedure(&my_out_server, call_ending_from_out, "call_ending_of_timeout", NULL);
    jrpc_register_procedure(&my_out_server, call_ending_of_otherinner, "call_ending_of_otherinner", NULL);
    jrpc_register_procedure(&my_out_server, video_record, "video_record", NULL);
    jrpc_register_procedure(&my_out_server, msg_push, "msgpush", NULL);
    jrpc_register_procedure(&my_out_server, update_push, "update_push", NULL);
    jrpc_register_procedure(&my_out_server, softupdate_innerbox, "softupdate_innerbox", NULL);
    jrpc_register_procedure(&my_out_server, softupdate_query, "softupdate_query", NULL);
    jrpc_register_procedure(&my_out_server, ipconf_updated, "ipconf_updated", NULL);
    jrpc_register_procedure(&my_out_server, start_shell, "start_shell", NULL);
    jrpc_register_procedure(&my_sec_server, seczone_emergency, "seczone_emergency", NULL);
    jrpc_register_procedure(&my_sec_server, show_version, "show_version", NULL);

    jrpc_register_procedure(&my_in_server, system_query_stat, "system_query_stat", NULL);
    jrpc_register_procedure(&my_in_server, system_reboot, "system_reboot", NULL);



    //heartbeat_to_property();

    printf("Server started ...(777)\n");

    server_run();

//	jrpc_server_run(&my_in_server);
//	jrpc_server_run(&my_out_server);
//	jrpc_server_run(&my_sec_server);

//	jrpc_server_destroy(&my_in_server);
//	jrpc_server_destroy(&my_out_server);
//	jrpc_server_destroy(&my_sec_server);
    return 0;
}
