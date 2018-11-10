/*
 * server.c
 *
 *  Created on: Oct 9, 2016
 *      Author: ztf
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "jsonrpc-c.h"
#include <sys/fcntl.h> // fcntl
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include <linux/ioctl.h>
#include <linux/watchdog.h>
#include <net/if.h>

#include "outerproc.h"
#include "iniparser.h"
#include "rc4.h"

#ifdef BEIJING
#include "elevator_control.h"
#endif

#define PORT 1234  // the port users will be connecting to
#define PROPERTY_PORT 18699
#define PID_PATH "/etc/serverpid"

#define STAT_NET_UP 0x1
#define STAT_CON_ON 0x2

unsigned char g_sys_stat = 0x3;

char g_localaddress[64];
char g_stream_outerbox[1024];
innerbox_info_t * g_innerbox_info_list;
int g_innerbox_count = 0;

basic_conf_t g_baseconf;

char g_call_from_outerbox[2048];	
char g_qrcode_opendoor_request_to_property[2048];
char g_bluetooth_opendoor_request_to_property[2048];
char g_opendoor_request[256];	
char g_callout_ipaddr[64];

#define BASIC_CONF "/etc/basic.conf"
#define INNERBOX_CONF "/etc/innerbox.conf"
#define FFPLAY_PID_PATH "/etc/ffplay.pid"

void query_info_from_property(char* cmdstr, struct ev_loop *loop);
void * ipaddr_query_from_property();
int send_msg_to_server(char * serveraddr,int port, char * cmd);

char g_ipquery_cmdstr[2048];
char *g_playsound_cmd = "/home/root/ffplay -nodisp /home/root/6513.wav -loop 5 &";
char *g_killplay_cmd = "killall -9 ffplay";
char g_urlstring[1024];

pthread_mutex_t lock;

//#define CALLING 1
//#define CLOSING 0

#define WRONG_PARAMETER "Wrong_parameters"

#if defined YANGLIUJUN
#define VERSION "1.0.%s.%d-mc-O"
#elif defined BEIJING
#define VERSION "1.0.%s.%d-bj-O"
#else
#define VERSION "1.0.%s.%d-O"
#endif

/*
 * This struct makes it possible to attempt a connection to each known
 * address for the echo server (as returned by getaddrinfo) -- one at
 * a time -- from the libev event loop, without blocking on
 * connect(). The eio member is the watcher of the latest connection
 * attempt. If it successfully connects, its file descriptor is the
 * one that should be passed to the client_session constructor as the
 * server file descriptor.
 */
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

//connect_watcher g_connectout_watcher;


ev_io * g_last_eio = NULL;
int g_opendoor_state = 0;
int g_ipconf_get_flag = 0;

typedef enum call_state {
	IDLE,
	CONNECTING,
	CALLING
}call_state_e;

typedef struct current_calling_state {
	char calling_ipaddr[128];//
	int calling_port;
	int calling_socket;
	call_state_e calling_state;
	time_t connect_start_time;
	time_t call_start_time;
	char audiostream[1024];
} current_calling_state_t;

current_calling_state_t g_current_calling_state;

ev_timer g_timeout_watcher;

static void create_worker(void *(*func)(), void *arg);

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
	DEBUG_INFO("connected peer address = %s:%d\n",peeraddress,port );

	return port;
}

void write_net_wrong()
{
	char *callend = "\xaa\x05\x10\xbf\x55";
	write_uart_cmd(callend,5);
}

void write_door_id() {
	char *callend = "\xaa\x05\x0f\xa0\x55";
	write_uart_cmd(callend,5);
}


static void timeout_cb (EV_P_ ev_timer *w, int revents) // Timer callback function
{
	send_msg_to_server("127.0.0.1",7890, "{\"method\":\"call_ending_from_local\"}");
  	DEBUG_INFO("in timeout_cb\n"); 
}

//when current player is aplay,and next player is ffplay,just wait util aplay exit
void kill_ffplay()
{

/* 	FILE *fp = fopen(FFPLAY_PID_PATH,"r");
    	int pid_to_be_check = 0;
    	if(fp) {
        	fscanf(fp,"%d",&pid_to_be_check);
       		fclose(fp);
    	}
*/
	char pidline[1024];
    	char cmdstr[128];
	char *pid = NULL;
	int i = 0;
	int pidno[64];
	FILE *fp = popen("pidof ffplay","r");
	if(!fp)
		return;
	bzero(pidline,1024);
	fgets(pidline,1024,fp);
        //snprintf(cmdstr,128,"kill -9 %d",pid_to_be_check);
        //system(cmdstr);
	
	pid = strtok(pidline," ");
	while(pid != NULL) {

		pidno[i] = atoi(pid);
		if(pidno[i]>0){
			snprintf(cmdstr,128,"kill -9 %d",pidno[i]);
			DEBUG_INFO("%s",cmdstr);
			system(cmdstr);
		}
		pid = strtok(NULL," ");
		i++;
	}
	pclose(fp);
	
        fp = popen("pidof aplay","r");
        fgets(pidline,1024,fp);
	pid = NULL;
        pid = strtok(pidline, " ");
	pclose(fp);
	while( pid != NULL) {
		usleep(100);
		pid = NULL;
        	fp = popen("pidof aplay","r");
		bzero(pidline,1024);
        	fgets(pidline,1024,fp);
        	pid = strtok(pidline, " ");
        	pclose(fp);
	}

/*
    	char cmdstr[128];
	if (pid_to_be_check==0 ||  kill(pid_to_be_check, 0) < 0  ) {
		system(g_killplay_cmd);
		system(g_killplay_cmd);
		system(g_killplay_cmd);
		return;
	}
*/
}

void * do_call_ending()
{

	int port = 0;
	pthread_detach(pthread_self());
	kill_ffplay();
	char callending[1024];
	bzero(callending,1024);
	
	snprintf(callending,1024,"{\"method\":\"call_ending\",\"params\":{\"ipaddr\":\"%s\"}}",g_localaddress);
	
	if(g_current_calling_state.calling_port > 0) {
		send_msg_to_server(g_current_calling_state.calling_ipaddr,g_current_calling_state.calling_port, callending);
	
		close(g_current_calling_state.calling_socket);
	}
	bzero(&g_current_calling_state,sizeof(g_current_calling_state));
	g_current_calling_state.calling_state = IDLE;
	
	bzero(g_current_calling_state.calling_ipaddr,128);	
	g_current_calling_state.calling_port = 0;
	g_current_calling_state.call_start_time = 0;
	g_current_calling_state.connect_start_time = 0;
	
//#ifdef BEIJING
	char *callend = "\xaa\x05\x0c\xa3\x55";
	write_uart_cmd(callend,5);
//#endif
}

int send_msg_to_server(char * serveraddr,int port, char * cmd)
{
        int    sockfd, n;
        char    recvline[4096], sendline[4096];
        struct sockaddr_in    servaddr;
        //char *cmd="{\"method\":\"opendoor_permit\"}\0";

        if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                DEBUG_ERR_LOG("create socket error");
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

 	struct timeval timeout={3,0};//3s
    	setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
    	setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));

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
                DEBUG_ERR_LOG("connect error");
		close(sockfd);
                return 0;
        }

        ul = 0;
        ioctl(sockfd, FIONBIO, &ul);

        //gets(sendline, 4096, stdin);

        if( send(sockfd, cmd, strlen(cmd), 0) < 0)
        {
                DEBUG_ERR_LOG("send msg error");
		close(sockfd);
                return 0;
        }
        DEBUG_INFO("sockfd:%d %s",sockfd,cmd);
        close(sockfd);
        return 1;
}



innerbox_info_t * malloc_innerbox_info()
{
	innerbox_info_t *newinnerbox = malloc(sizeof(innerbox_info_t));
	bzero(newinnerbox,sizeof(innerbox_info_t));
	newinnerbox->next = NULL;

	return newinnerbox;	
}

void insert_into_innerbox_info_list(char* doorid,char *ipaddr)
{
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

innerbox_info_t * select_from_innerbox_info_list(char *doorid)
{
	innerbox_info_t * loop = g_innerbox_info_list;
	while(loop) {
		if(strcmp(doorid,loop->doorid) == 0)
			return loop;
		loop = loop->next;
	}
	return NULL;
}

void create_basic_conf_file(void)
{
        FILE    *   ini ;
	int i = 0;

        ini = fopen(BASIC_CONF, "w");
	if(ini) {
        fprintf(ini,
        "#\n"
        "# basic configure file\n"
        "#\n"
        "\n"
        "[outerinterface]\n"
        "\n"
        "ip = %s ;\n"
        "\n"
        "\n"
		"#物业中心服务器\n"
		"#\n"
        "[propertyserver]\n"
        "\n"
        "ip = %s ;\n"
		"#物业呼叫中心\n"
		"#\n"
        "[callcenter]\n"
        "\n"
        "ip = %s ;\n"
        "\n"
        "\n"
		"#门禁控制器\n"
		"#\n"
        "[doorcontroller]\n"
        "\n"
        "ip = %s ;\n"
		"serialno = %u ;\n"
        "\n"
        "\n"
        "#物业岗亭机\n"
        "#\n"
        "[sentrybox]\n"
        "\n"
        "count = %d ;\n"
        "\n"
        "\n",
        g_baseconf.outerinterfaceip,
        g_baseconf.propertyip,
        g_baseconf.callcenterip,
		g_baseconf.dcip,
		g_baseconf.dcserial,
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
	}
}

int get_baseconf_fromini(char * ini_name)
{
        dictionary  *   ini ;

        /* Some temporary variables to hold query results */
        int             b ;
        int             i ;
        double          d ;
        char        *   s ;
	int j = 0;
	char tmpstr[128];

        ini = iniparser_load(ini_name);
        if (ini==NULL) {
                fprintf(stderr, "cannot parse file: %s\n", ini_name);
                return -1 ;
        }

        /* Get attributes */


        DEBUG_INFO("outerinterface");
        s = iniparser_getstring(ini, "outerinterface:ip", NULL);
        DEBUG_INFO("IP: [%s]\n", s ? s : "UNDEF");
        if(s)
                strcpy(g_baseconf.outerinterfaceip,s);

        DEBUG_INFO("property:\n");
        s = iniparser_getstring(ini, "propertyserver:ip", NULL);
        DEBUG_INFO("IP: [%s]\n", s ? s : "UNDEF");
        if(s)
                strcpy(g_baseconf.propertyip,s);

        DEBUG_INFO("doorcontroller:\n");
        s = iniparser_getstring(ini, "doorcontroller:ip", NULL);
        DEBUG_INFO("IP: [%s]\n", s ? s : "UNDEF");
        if(s)
                strcpy(g_baseconf.dcip,s);

        g_baseconf.dcserial = iniparser_getint(ini, "doorcontroller:serialno", 0);
        DEBUG_INFO("serial: [%u]\n", g_baseconf.dcserial );

        g_baseconf.dcid = iniparser_getint(ini, "doorcontroller:doorid", 1);
        DEBUG_INFO("doorid: [%u]\n", g_baseconf.dcid );

        DEBUG_INFO("callcenter:");
        s = iniparser_getstring(ini, "callcenter:ip", NULL);
        DEBUG_INFO("IP: [%s]", s ? s : "UNDEF");
        if(s)
                strcpy(g_baseconf.callcenterip,s);
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

        iniparser_freedict(ini);

        if((strlen(g_baseconf.callcenterip) == 0) && (strlen(g_baseconf.propertyip)>0))
                strcpy(g_baseconf.callcenterip,g_baseconf.propertyip);
        return 0 ;
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


void get_basic_conf()
{
        memset(g_baseconf.outerinterfaceip,64,0);
        memset(g_baseconf.propertyip,64,0);
        get_baseconf_fromini(BASIC_CONF);

	char compiletime[16];
        bzero(compiletime,16);
        int hour = get_compile_time(compiletime);
        snprintf(g_baseconf.version,64,VERSION,compiletime,hour);
	
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

                if (ifAddrStruct->ifa_addr->sa_family==AF_INET) {
                        tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
                        char addressBuffer[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                        DEBUG_INFO("%s IP Address %s", ifAddrStruct->ifa_name, addressBuffer);
                        if(strncmp(ifAddrStruct->ifa_name,"eth0",4) == 0)
                                strcpy(g_localaddress,addressBuffer);
                } else if (ifAddrStruct->ifa_addr->sa_family==AF_INET6) {
                        tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
                        char addressBuffer[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
                        DEBUG_INFO("%s IP Address %s", ifAddrStruct->ifa_name, addressBuffer);
                }        ifAddrStruct=ifAddrStruct->ifa_next;
        }
	freeifaddrs(ifAddrStruct);
}

void init_stream_url()
{

        memset(g_stream_outerbox,0,1024);
        //sprintf(urlstring,"rtsp://%s:554/ch01_sub.h264",g_localaddress);
        //sprintf(urlstring,"rtsp://%s:554/user=admin_password=tlJwpbo6_channel=1_stream=1.sdp?real_stream",g_localaddress);
        snprintf(g_stream_outerbox,1024,"rtsp://admin:admin@%s:554/stream1",g_localaddress);
	snprintf(g_call_from_outerbox,2048,"{\"method\":\"call_from_outerbox\",\"params\":{\"rtspurl\":\"%s\",\"ipaddr\":\"%s\"}}",g_stream_outerbox,g_localaddress);

}

void create_innerbox_info_conf(void)
{
	FILE    *   ini ;
	innerbox_info_t * loop = g_innerbox_info_list;

    	ini = fopen(INNERBOX_CONF, "w");
	if(ini) {
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
	}
//	fflush(ini);

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

/*serial start*/

int set_serial_attr(int sp_fd, int band_rate, char data_bits, char odd_even, char stop_bits)
{
        struct termios opt;

        if (-1 == sp_fd)
        {
                return -1;
        }

        bzero(&opt, sizeof(struct termios));

        if (-1 == tcgetattr(sp_fd, &opt))
        {
                return -1;
        }

        opt.c_cflag |= CLOCAL | CREAD;

        opt.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        opt.c_oflag &= ~OPOST;
        opt.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

        switch (band_rate)
        {
        case 600:
                if (-1 == cfsetospeed(&opt, B600))
                {
                        return -1;
                }
                if (-1 == cfsetispeed(&opt, B600))
                {
                        return -1;
                }
                break;
        case 1200:
                if (-1 == cfsetospeed(&opt, B1200))
                {
                        return -1;
                }
                if (-1 == cfsetispeed(&opt, B1200))
                {
                        return -1;
                }
                break;
        case 2400:
                if (-1 == cfsetospeed(&opt, B2400))
		{
                        return -1;
                }
                if (-1 == cfsetispeed(&opt, B2400))
                {
                        return -1;
                }
                break;
        case 4800:
                if (-1 == cfsetospeed(&opt, B4800))
                {
                        return -1;
                }
                if (-1 == cfsetispeed(&opt, B4800))
                {
                        return -1;
                }
                break;
        case 9600:
                if (-1 == cfsetospeed(&opt, B9600))
                {
                        return -1;
                }
                if (-1 == cfsetispeed(&opt, B9600))
                {
                        return -1;
                }
                break;
        case 19200:
                if (-1 == cfsetospeed(&opt, B19200))
                {
                        return -1;
                }
                if (-1 == cfsetispeed(&opt, B19200))
                {
                        return -1;
                }
                break;
        case 38400:
                if (-1 == cfsetospeed(&opt, B38400))
                {
                        return -1;
                }
                if (-1 == cfsetispeed(&opt, B38400))
                {
                        return -1;
                }
                break;
        case 57600:
                if (-1 == cfsetospeed(&opt, B57600))
                {
                        return -1;
                }
                if (-1 == cfsetispeed(&opt, B57600))
                {
                        return -1;
                }
                break;
        case 115200:
                if (-1 == cfsetospeed(&opt, B115200))
                {
                        return -1;
                }
                if (-1 == cfsetispeed(&opt, B115200))
                {
                        return -1;
                }
                break;
        default:
                if (-1 == cfsetospeed(&opt, B115200))
                {
                        return -1;
                }
                if (-1 == cfsetispeed(&opt, B115200))
                {
                        return -1;
                }
        }

        switch (data_bits)
        {
        case 5:
                opt.c_cflag &= ~CSIZE;
                opt.c_cflag |= CS5;
                break;
        case 6:
                opt.c_cflag &= ~CSIZE;
                opt.c_cflag |= CS6;
                break;
        case 7:
                opt.c_cflag &= ~CSIZE;
                opt.c_cflag |= CS7;
                break;
        case 8:
        default:
                opt.c_cflag &= ~CSIZE;
                opt.c_cflag |= CS8;
        }

        switch (odd_even)
        {
        case 'n':
        case 'N':
                opt.c_cflag &= ~PARENB;
                break;
        case 'o':
        case 'O':
                opt.c_cflag |= INPCK;
                opt.c_cflag |= PARENB;
                opt.c_cflag |= PARODD;
                break;
        case 'e':
        case 'E':
                opt.c_cflag |= INPCK;
                opt.c_cflag |= PARENB;
                opt.c_cflag &= ~PARODD;
                break;
        default:
                opt.c_cflag &= ~PARENB;
        }

        switch (stop_bits)
        {
        case 1:
                opt.c_cflag &= ~CSTOPB;
                break;
        case 2:
                opt.c_cflag |= CSTOPB;
                break;
        default:
                opt.c_cflag &= ~CSTOPB;
        }

        if (-1 == tcflush(sp_fd, TCIOFLUSH))
        {
                return -1;
        }

        if (-1 == tcsetattr(sp_fd, TCSANOW, &opt))
        {
                return -1;
        }

        return 0;
}

int write_uart_cmd(char *data,int len)
{

#if defined BEIJING
	char usart_string[] = "/dev/ttyO2";
#elif defined YANGLIUJUN
	char usart_string[] = "/dev/ttyO4";
#else
	char usart_string[] = "/dev/ttyO2";
#endif
	int  fd ;
	int  wr_bytes;
	int i = 0;
	int ret = 0;

	unsigned int write_len ;

	fd = open(usart_string,O_WRONLY);
	if(fd == -1)
	{
		DEBUG_ERR_LOG("Open error %s -1...", usart_string);
		        return -1;
	}
	DEBUG_INFO("open :%d",fd);
#ifdef BEIJING
	ret = set_serial_attr(fd, 9600, 8, 'n', 1);
#endif 
#ifdef YANGLIUJUN
	ret = set_serial_attr(fd, 115200, 8, 'n', 1);
#endif	
	if(ret != 0)
	{
		DEBUG_ERR_LOG("set_serial_attr error ret= %d...", ret);
		        return -2;
	}

	for(i = 0;i<len;i++){
		wr_bytes = write(fd,data+i,1);
		if(wr_bytes != 1){
		        i--;
		        DEBUG_ERR_LOG("write error");
		        continue;
		}
		ret++;
	    }

	close(fd);

	return ret;
}
/*serial end*/

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

static void connect_cb (EV_P_ ev_io *w, int revents)
{
	int remote_fd =  w->fd;
	connect_watcher * watcher = (connect_watcher *)(w->data);
	char *line = watcher->data;
//	char *line = "this is a test";
	int len = strlen(line);
	size_t bytes_read = 0;
	if (revents & EV_WRITE)
	{
		puts ("remote ready for writing...");
		DEBUG_INFO("recv string:%s",line);
		if (-1 == send(remote_fd, line, len, 0)) {
			DEBUG_INFO("calling_state:%d",g_current_calling_state.calling_state);
			if(g_current_calling_state.calling_state == CONNECTING); {
				write_net_wrong();
				g_current_calling_state.calling_state = IDLE; 
				
			}
			//ev_io_stop(EV_A_ w);
		      	DEBUG_ERR_LOG("echo send");
			//shutdown(remote_fd,SHUT_RDWR);
                        ev_io_stop(EV_A_ w);
                        close(remote_fd);
                        ev_loop_destroy(loop);
			free(watcher->buffer);
			free(watcher);
		      	//DEBUG_ERR_LOG("shutdown %d",close(remote_fd));

			g_sys_stat &= ~STAT_CON_ON;
		      	return ;
		}

		if(strcmp(line,"{\"method\":\"opendoor\"}") == 0) {
			
			//system(g_killplay_cmd);
	//		kill_ffplay();
         //               system(g_playsound_cmd);
		}
		    // once the data is sent, stop notifications that
		    // data can be sent until there is actually more 
		    // data to send
		ev_io_stop(EV_A_ w);
		ev_io_set(EV_A_ w, remote_fd, EV_READ);
		ev_io_start(EV_A_ w);
		g_last_eio = w;
	}
	else 
	if (revents & EV_READ)
	{
/*		int n;
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
		DEBUG_INFO("current send file fd:%d",w->fd);
		int fd = w->fd;
		if (watcher->pos == (watcher->buffer_size - 1)) {
			char * new_buffer = realloc(watcher->buffer, watcher->buffer_size *= 2);
			if (new_buffer == NULL) {
				DEBUG_ERR_LOG("Memory error");
				free(watcher->buffer);
				free(watcher);
				ev_io_stop(EV_A_ w);
				close(fd);
                        	ev_loop_destroy(loop);
				return;
//				return close_connection(loop, w);
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
                        ev_loop_destroy(loop);
			free(watcher->buffer);
			free(watcher);
			if(errno == EAGAIN || errno == 9){
				DEBUG_ERR_LOG("connect_cb read error and exit thread");
				g_sys_stat &= ~STAT_CON_ON;
				int pid = getpid();
				//kill(pid,9);
				pthread_exit(0);
			}
			g_sys_stat &= ~STAT_CON_ON;
			DEBUG_ERR_LOG("connect_cb read error");
			//ev_unloop(EV_A_ EVUNLOOP_ONE);
			return;
			//return close_connection(loop, w);
		}
		if (!bytes_read) {
			// client closed the sending half of the connection
			DEBUG_ERR_LOG("Client closed connection.");
			ev_io_stop(EV_A_ w);
			close(fd);
                        ev_loop_destroy(loop);
			g_sys_stat &= ~STAT_CON_ON;
			free(watcher->buffer);
			free(watcher);
			return;			
			//return close_connection(loop, w);
		} else {
			cJSON *root;
			char *end_ptr = NULL;
			watcher->pos += bytes_read;
			g_sys_stat |= STAT_CON_ON;

			if ((root = cJSON_Parse_Stream(watcher->buffer, &end_ptr)) != NULL) {
				if ( 1) {
					char * str_result = cJSON_Print(root);
					DEBUG_INFO("Valid JSON Received:\n%s", str_result);
					free(str_result);
				}

				if (root->type == cJSON_Object) {
					//eval_request(server, conn, root);
					cJSON * result, *audio;
					result = cJSON_GetObjectItem(root, "result");
					//DEBUG_INFO("result:%s",cJSON_Print(result));

				        if(result != NULL)
                				if(result->type == cJSON_String) {
							if(strcmp(result->valuestring,"opendoor_request_sent") == 0) {
								DEBUG_INFO("#####opendoor_request_sent cmd get");
								kill_ffplay();
                                				system(g_playsound_cmd);
							}
	
							if(strcmp(result->valuestring,"call_accept") == 0) {
								audio = cJSON_GetObjectItem(root,"audio");
								if( audio != NULL)
									if(audio->type == cJSON_String) {
										char cmdstr[1024];
										bzero(cmdstr,1024);
										snprintf(cmdstr,1024,"/home/root/ffplay -nodisp -probesize 32 %s &",audio->valuestring);
										DEBUG_INFO("%s\n",cmdstr);

//#ifdef BEIJING
										char *iscalling = "\xaa\x05\x0b\xa4\x55";
										write_uart_cmd(iscalling,5);
//#endif
										//system(g_killplay_cmd);
										kill_ffplay();
										system(cmdstr);
     
									//	pthread_mutex_lock(&lock);
										g_current_calling_state.calling_state = CALLING;
										strncpy(g_current_calling_state.audiostream,audio->valuestring,1024);	
										time_t now = time(&now);
										g_current_calling_state.call_start_time=now;
									//	pthread_mutex_unlock(&lock);
									}
							}
	
							if(strcmp(result->valuestring,"line_is_busy") == 0) {
								//system(g_killplay_cmd);
								kill_ffplay();
								//system(g_playsound_cmd);
								char *callend = "\xaa\x05\x0c\xa3\x55";
								char cmdstr[1024];
								bzero(cmdstr,1024);
								snprintf(cmdstr,1024,"/home/root/ffplay -nodisp /home/root/ring_busy.mp3 &",audio->valuestring);
								system(cmdstr);

//#ifdef BEIJING
								write_uart_cmd(callend,5);							
//#endif	
								//pthread_mutex_lock(&lock);
								g_current_calling_state.calling_state = IDLE;
								//pthread_mutex_unlock(&lock);
							}
							
							if(strcmp(result->valuestring,"ipconf_of_innerbox") == 0) {
								cJSON *ipaddrarray = cJSON_GetObjectItem(root,"values");
								cJSON *item,*doorid,*ipaddr;
								
								if(ipaddrarray != NULL && ipaddrarray->type == cJSON_Array) {
									g_innerbox_count = cJSON_GetArraySize(ipaddrarray);
									g_ipconf_get_flag = g_innerbox_count;
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
		
							if(strcmp(result->valuestring,"qrcode_opendoor_ret") == 0) {
								cJSON *qrret = cJSON_GetObjectItem(root,"ret");
								if(qrret != NULL && qrret->type == cJSON_Number) {
									switch (qrret->valueint) {
										case 1:
											//dor opendoor
											DEBUG_INFO("qrcode opendoor cmd get");
											break;
										case 2:
											//send message to io
											DEBUG_INFO("qrcode opendoor deny");
											break;
										case 3:
											//send message to io
											DEBUG_INFO("qrcode not true");
											break;
										default:
											break;
									}
								}
							}

							if(strcmp(result->valuestring,"bluetooth_opendoor_ret") == 0) {
								cJSON *qrret = cJSON_GetObjectItem(root,"ret");
								if(qrret != NULL && qrret->type == cJSON_Number) {
									switch (qrret->valueint) {
										case 1:
											//dor opendoor
											break;
										case 0:
											//send message to io
											break;
										default:
											break;
									}
								}
							}



						}

				}
				//shift processed request, discarding it
				memmove(watcher->buffer, end_ptr, strlen(end_ptr) + 2);

				watcher->pos = strlen(end_ptr);
				memset(watcher->buffer + watcher->pos, 0,
						watcher->buffer_size - watcher->pos - 1);

				cJSON_Delete(root);
				ev_io_stop(EV_A_ w);
				close(fd);
                        	ev_loop_destroy(loop);
				free(watcher->buffer);
				free(watcher);
				
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
                        		ev_loop_destroy(loop);
					free(watcher->buffer);
					free(watcher);
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

	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_family = AF_INET;

	DEBUG_INFO("callout_ipaddr:%s",addrstr);
	int err = getaddrinfo(addrstr,
		                  portstr,
		                  &hints,
		                  &res);

	struct addrinfo *addr,*addr_base;
	addr = res;
	addr_base =  res;

	if (err) {
		DEBUG_ERR_LOG( "getaddrinfo failed: %s", gai_strerror(err));
		g_sys_stat &= ~STAT_CON_ON;
		return NULL;
	}

	int fd = socket(addr->ai_addr->sa_family, SOCK_STREAM, 0);
	if (fd == -1) {
		g_sys_stat &= ~STAT_CON_ON;
		return NULL;
	}

	DEBUG_INFO("NON_BLOCK_SOCKET open%d",fd);
/*
	if(block == 1)
		if (set_nonblocking(fd) == -1)
			goto err_cleanup;
*/	    
	/*
	  * Treat both an immediate connection and EINPROGRESS as success,
	  * let the caller sort it out.
	  */
	int status = connect(fd, addr->ai_addr, addr->ai_addrlen);
	DEBUG_INFO("connect status #####%d",status);
	if ((status == 0) || ((status == -1) && (errno == EINPROGRESS))) {
		connect_watcher *c = malloc(sizeof(connect_watcher));
		if (c) 
		{
			    bzero(c,sizeof(connect_watcher));	
			    ev_io_init(&c->eio, connect_cb, fd, EV_WRITE);
			    c->addr = addr;
			    c->addr_base = addr_base;
			    g_sys_stat |= STAT_CON_ON;
			    return c;
		}
		/* else error */
	}
err_cleanup:
		
	DEBUG_INFO("closing*****%d",fd);
	g_sys_stat &= ~STAT_CON_ON;
        ev_break(loop,EVBREAK_ONE);
        ev_unref(loop);
        ev_loop_destroy(loop);
       	close(fd);
	DEBUG_INFO("closing~~~~~%d",fd);
	return NULL;
}

struct jrpc_server my_out_server;
char * opencmd = "open the door please!\n";

cJSON * say_hello(jrpc_context * ctx, cJSON * params, cJSON *id) {
	return cJSON_CreateString("Hello!");
}

#ifdef YANGLIUJUN
int opendoor_udp(){

        int s,len;
        struct sockaddr_in addr;
        int addr_len = sizeof(struct sockaddr_in);
        char sbuffer[128];
        char rbuffer[128];

        memset(sbuffer,0,128);
        sbuffer[0] = 0x17;
        sbuffer[1] = 0x40;
        sbuffer[2] = 0;
        sbuffer[3] = 0;
        sbuffer[4] = 0xbc;
        sbuffer[5] = 0x0b;
        sbuffer[6] = 0x38;
        sbuffer[7] = 0x19;
        sbuffer[8] = g_baseconf.dcid; //doorid
	
	memcpy(sbuffer+4,&g_baseconf.dcserial,4);

        if((s = socket(AF_INET,SOCK_DGRAM,0)) < 0){
                perror("udp socket");
                return -1;
        }

        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(60000);
        addr.sin_addr.s_addr = inet_addr(g_baseconf.dcip);

        //snprintf(buffer,1024,"{\"method\":\"heartbeat\"}");

        set_nonblocking(s);

        sendto(s,sbuffer,64,0,(struct sockaddr *) &addr, addr_len);

        bzero(rbuffer,128);
        usleep(1000);
        len = recvfrom(s,rbuffer,64,0,(struct sockaddr *)&addr,&addr_len);
	
        //DEBUG_INFO("receive :%s %d",buffer,len);
        close(s);

	sbuffer[8] =0x1;
	
	if(memcmp(sbuffer,rbuffer,64) != 0)
		return -1;	
	return 0;
}
#endif


cJSON * opendoor_permit(jrpc_context * ctx, cJSON * params, cJSON *id) {

        //do something
	//system(g_killplay_cmd);

#ifdef BEIJING
	int ret = 0;
	char *opendoor = "\xaa\x07\x0a\x01\x01\xa7\x55";

	ret = write_uart_cmd(opendoor,7);
	if(ret <= 0){
		DEBUG_ERR("opendoor_failed");
		return cJSON_CreateString("opendoor_failed");
	}	
/*	
	while(ret <= 0) {	
		ret = write_uart_cmd(opendoor,7);
		DEBUG_INFO("opendoor permit:%d",ret);
		usleep(1);
	}
*/
#endif

#ifdef YANGLIUJUN
	if(opendoor_udp() == -1 ) {

		DEBUG_ERR("opendoor_failed");
		return cJSON_CreateString("opendoor_failed");
	}
#endif
	kill_ffplay();
	system("/usr/bin/aplay /home/root/14.wav &");

	
	if(g_current_calling_state.calling_state == CALLING) {
		
		char playcmd[2048];
		bzero(playcmd,2048);
		snprintf(playcmd,2048,"/home/root/ffplay -nodisp -probesize 32 %s & \n",g_current_calling_state.audiostream);
		kill_ffplay();
		system(playcmd);		
	}

        return cJSON_CreateString("opendoor_is_done");

}

//挂断
cJSON * call_ending(jrpc_context * ctx, cJSON * params, cJSON *id) {
	//system(g_killplay_cmd);
	kill_ffplay();
	
//#ifdef BEIJING
	char *callend = "\xaa\x05\x0c\xa3\x55";
	write_uart_cmd(callend,5);
//#endif
	//pthread_mutex_lock(&lock);
	close(g_current_calling_state.calling_socket);
	bzero(&g_current_calling_state,sizeof(g_current_calling_state));
	g_current_calling_state.calling_state = IDLE;
	//pthread_mutex_unlock(&lock);

	//close(g_last_sendfd);//for bad file descriptor
        return cJSON_CreateString("call_ending_is_done");
}

cJSON * call_ending_from_local(jrpc_context * ctx, cJSON * params, cJSON *id) {
	DEBUG_INFO("current_calling_state:%d",g_current_calling_state.calling_state);
/*
	if(g_current_calling_state.calling_state == IDLE){
		
		kill_ffplay();
        	return cJSON_CreateString("state_idle");
	}
*/
	create_worker(do_call_ending,0);
	//system(g_killplay_cmd);
	//close(g_last_sendfd);//for bad file descriptor
        return cJSON_CreateString("call_ending_is_done");
}

cJSON * opendoor_deny(jrpc_context * ctx, cJSON * params, cJSON *id) {

	//system(g_killplay_cmd);
	kill_ffplay();

//#ifdef BEIJING
	char *callend = "\xaa\x05\x0c\xa3\x55";
	write_uart_cmd(callend,5);
//#endif	
	//pthread_mutex_lock(&lock);
	bzero(&g_current_calling_state,sizeof(g_current_calling_state));
	g_current_calling_state.calling_state = IDLE;
	//pthread_mutex_unlock(&lock);

        return cJSON_CreateString("opendoor_deny_has_done");
}
//正在通话中
cJSON * play_live_audio(jrpc_context * ctx, cJSON * params, cJSON *id) {

	cJSON * param = cJSON_GetObjectItem(params, "streamurl");
	char cmd[1024];
	bzero(cmd,1024);

	if(param != NULL && param->type == cJSON_String) {
		snprintf(cmd,1024,"/home/root/ffplay -nodisp -probesize 32 %s &",param->valuestring);
		strncpy(g_current_calling_state.audiostream,param->valuestring,1024);
	}
	else {
		snprintf(cmd,1024,"/home/root/ffplay -nodisp -probesize 32 rtmp://127.0.0.1:1935/live2 &");
		strncpy(g_current_calling_state.audiostream,"rtmp://127.0.0.1:1935/live2",1024);
	}		
	//printf("accept the opendoor");	
//	system(g_killplay_cmd);
	system(cmd);
	//pthread_mutex_lock(&lock);
	g_current_calling_state.calling_state = CALLING;
	time_t now = time(&now);
	g_current_calling_state.call_start_time=now;
	//pthread_mutex_unlock(&lock);
//#ifdef BEIJING
	char *iscalling = "\xaa\x05\x0b\xa4\x55";
	write_uart_cmd(iscalling,5);
//#endif
	DEBUG_INFO("get play live audio");

        return cJSON_CreateString("play_live_audio_has_done");
}

cJSON * exit_server(jrpc_context * ctx, cJSON * params, cJSON *id) {
	jrpc_server_stop(&my_out_server);
	return cJSON_CreateString("Bye!");
}

cJSON * ipconf_updated(jrpc_context * ctx, cJSON * params, cJSON *id) {
	//char cmdstr[2048];

	create_worker(ipaddr_query_from_property,0);
	//query_info_from_property(g_ipquery_cmdstr,loop_read);
	return NULL;
}

void * do_call_property()
{
	char portstr[16];
        bzero(portstr,16);
	
	pthread_detach(pthread_self());
	
//	struct ev_loop *loopinthread =ev_loop_new(0);
	
	//ev_run(loopinthread,0);
	
        snprintf(portstr,16,"%d",PROPERTY_PORT);
	struct ev_loop *loopinthread =ev_loop_new(0);
        connect_watcher *c = new_connector(loopinthread, g_callout_ipaddr, portstr);
        if(!c) {
                DEBUG_ERR_LOG("Cannot connect to property server %s:%d",g_callout_ipaddr,PROPERTY_PORT);
		write_net_wrong();
                return cJSON_CreateString("cannot_connect_to_property");
        }

        //memset(&g_connectout_watcher,0,sizeof(struct connect_watcher));
        c->buffer_size = 1500;
        if(!c->buffer)
                c->buffer = malloc(1500);
        memset(c->buffer, 0, 1500);
        c->pos = 0;

        //write command to property 
        c->eio.data = c;
        c->data = g_call_from_outerbox;

        //int ret =  write(c->eio.fd,call_from_outerbox,strlen(call_from_outerbox));
        if (c) {
                DEBUG_INFO(
                "Trying connection to %s:%d...",g_callout_ipaddr,PROPERTY_PORT
                );

		//pthread_mutex_lock(&lock);
		g_current_calling_state.calling_state =  CONNECTING;
		g_current_calling_state.calling_port =  PROPERTY_PORT;
		strcpy(g_current_calling_state.calling_ipaddr,g_callout_ipaddr);
		time_t now = time(&now);
		g_current_calling_state.connect_start_time=now;
		g_current_calling_state.calling_socket = c->eio.fd;
		//pthread_mutex_unlock(&lock);

                system(g_playsound_cmd);
                //ev_io_stop(loopinthread,&c->eio);
                ev_io_start(loopinthread, &c->eio);
		ev_run(loopinthread,0);
		

		//timer
		//ev_timer_init (&g_timeout_watcher, timeout_cb, 30, 0.); // Non repeating timer. The timer starts repeating in the timeout callback function
    		//ev_timer_start (loop_read, &g_timeout_watcher);
        }
        else {
                DEBUG_ERR_LOG( "Can't create a new connection");
 //               return cJSON_CreateString("cannot_connect_to_proerty");
        }

//        return cJSON_CreateString("call_property_is_done");

}

cJSON * calling_state(jrpc_context * ctx, cJSON * params, cJSON *id) {

	if(g_current_calling_state.calling_state != CALLING) {
		return cJSON_CreateString("line_is_not_busy");
	}else
		return cJSON_CreateString("line_is_busy");
}

cJSON * call_property(jrpc_context * ctx, cJSON * params, cJSON *id) {
	
	//close(g_connectout_watcher.eio.fd);
	if(g_current_calling_state.calling_state != IDLE) {
		return cJSON_CreateString("line_is_busy");
	}

        DEBUG_INFO("********");
	bzero(g_callout_ipaddr,64);
        cJSON * param = NULL;
        if(params != NULL)
                param = cJSON_GetObjectItem(params, "ipaddr");
        if(param != NULL && param->type == cJSON_String)
                snprintf(g_callout_ipaddr,64,"%s",param->valuestring);
        else
                snprintf(g_callout_ipaddr,64,"%s",g_baseconf.callcenterip);
	
        DEBUG_INFO("callout_ipaddr:%s",g_callout_ipaddr);
	create_worker(do_call_property,0);

	return cJSON_CreateString("call_property_is_done");
}

void * do_gate_opendoor_request(char *ipaddr) {
	
	//close(g_connectout_watcher.eio.fd);
/*	char portstr[16];
	bzero(portstr,16);
	snprintf(portstr,16,"%d","5678");
*/
	
	pthread_detach(pthread_self());
	char * portstr = "5678";
	struct ev_loop *loopinthread =ev_loop_new(0);
//	close(g_last_sendfd);//for bad file descriptor
	connect_watcher *c = new_connector(loopinthread,ipaddr, portstr);
	if(!c) {
		DEBUG_ERR_LOG("Cannot connect to server %s:%d",ipaddr,5678);
		return cJSON_CreateString("cannot_connect_to_inner");
	}
	//memset(&g_connectout_watcher,0,sizeof(struct connect_watcher));
	c->buffer_size = 1500;
	if(!c->buffer)
		c->buffer = malloc(1500);
	memset(c->buffer, 0, 1500);
	c->pos = 0;

	//write command to property 
	c->eio.data = c;
	c->data = g_opendoor_request;

 	//int ret =  write(c->eio.fd,call_from_outerbox,strlen(call_from_outerbox));
	if (c) {
		DEBUG_INFO( 
		"Trying connection to %s:%d...",ipaddr,5678
		);

		//g_opendoor_state = CALLING;

		//pthread_mutex_lock(&lock);
		g_current_calling_state.calling_state = CONNECTING;
		strcpy(g_current_calling_state.calling_ipaddr,ipaddr);
		time(&g_current_calling_state.connect_start_time);
		g_current_calling_state.calling_port =  5678;
		g_current_calling_state.calling_socket = c->eio.fd;
		//pthread_mutex_unlock(&lock);
		
//		ev_io_stop(loopinthread,&c->eio);
		ev_io_start(loopinthread, &c->eio);
		ev_run(loopinthread,0);

	}
	else {
	       	DEBUG_ERR_LOG( "Can't create a new connection");
		return cJSON_CreateString("cannot_connect_to_inner");
	}

	return cJSON_CreateString("opendoor_request_is_done");
}

cJSON * gate_opendoor_request(jrpc_context * ctx, cJSON * params, cJSON *id) {


	if(g_current_calling_state.calling_state != IDLE) {
		return cJSON_CreateString("opendoor_request_has_done_just_waiting");
	}

	innerbox_info_t *loop;

	cJSON *item;
	if(params!=NULL) {
		item = cJSON_GetObjectItem(params,"doorid");
	
		if(strcmp(item->valuestring,"99-99-9990") == 0) {
			DEBUG_ERR_LOG("get gate opendoor request 99-99-9990 reboot now");
			int fd = open("/dev/watchdog",O_WRONLY);
                        int data = 1;
                        ioctl (fd, WDIOC_SETTIMEOUT, &data);
                        ioctl (fd, WDIOC_GETTIMEOUT, &data);
                        while(1)
                          sleep(2);
			system("reboot");
		}

		loop=g_innerbox_info_list;
			
	//get the address first
		if(item != NULL && item->type == cJSON_String){

			loop =  select_from_innerbox_info_list(item->valuestring);
/*
			while(loop) {
				if(strcmp(loop->doorid,item->valuestring) == 0)
					break;
				loop = loop->next;
			}			
*/
		}
	}
	else
		return cJSON_CreateString("null_params");

	if(loop == NULL) {
		DEBUG_ERR_LOG("innerbox ip not found");
		g_current_calling_state.calling_state = IDLE;
		return cJSON_CreateString("innerbox_ip_not_found");
	}	
//	strcpy(portstr,"5678");	
	DEBUG_INFO("opendoor to:%s",loop->ipaddr);

	create_worker(do_gate_opendoor_request,loop->ipaddr);

	return cJSON_CreateString("opendoor_request_is_done");
}

void * do_softupdate()
{
	pthread_detach(pthread_self());
        char cmdstr[512];
        bzero(cmdstr,512);
        //wget the update package
        chdir("/");
        snprintf(cmdstr,512,"wget -O outer_update_package.tgz %s",g_urlstring);
        system(cmdstr);
	DEBUG_INFO("cmdstr %s",cmdstr);
        rc4_crypt_file("outer_update_package.tgz","outer_update_package_dec.tgz");
        system("tar zxvf outer_update_package_dec.tgz");
        system("rm -rf outer_update_package*.tgz");
        system("/tmp/outer_update.sh");
}

cJSON * softupdate_outerbox(jrpc_context * ctx, cJSON * params, cJSON *id) {

	
        cJSON *param = NULL;
        char version[32];
        bzero(version,32);
#ifdef DEBUG
        char * str = cJSON_Print(params);
        DEBUG_INFO("outer update:%s",str);
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
	create_worker(do_softupdate,g_urlstring);
	
	return cJSON_CreateString("doing_softupdate_of_outerbox_just_waiting");
}

void * do_opendoor_request(char *ipaddr) {
	
	//close(g_connectout_watcher.eio.fd);
	/*char portstr[16];
	bzero(portstr,16);
	snprintf(portstr,16,"%d",5678);*/

	pthread_detach(pthread_self());

	char *portstr = "5678";	
	struct ev_loop *loopinthread =ev_loop_new(0);
	
	connect_watcher *c = new_connector(loopinthread,ipaddr, portstr);
	if(!c) {
		DEBUG_ERR_LOG("Cannot connect to innerbox %s %s",ipaddr,portstr);
		write_net_wrong();
		return cJSON_CreateString("cannot_connect_to_innerbox");
	}
	c->buffer_size = 1500;
	if(!c->buffer)
		c->buffer = malloc(1500);
	memset(c->buffer, 0, 1500);
	c->pos = 0;

	//write command to property 
	c->eio.data = c;
	c->data = g_opendoor_request;

 	//int ret =  write(c->eio.fd,call_from_outerbox,strlen(call_from_outerbox));
	if (c) {
		DEBUG_INFO( 
		"Trying connection to innerbox %d...",5678
		);

		//pthread_mutex_lock(&lock);
		g_current_calling_state.calling_state = CONNECTING;
		g_current_calling_state.calling_port = 5678;
		strcpy(g_current_calling_state.calling_ipaddr,ipaddr);
		time(&g_current_calling_state.connect_start_time);
		g_current_calling_state.calling_socket = c->eio.fd;
		//pthread_mutex_unlock(&lock);

		ev_io_start(loopinthread, &c->eio);
		ev_run(loopinthread,0);
	}
	else {
		write_net_wrong();
	       	DEBUG_ERR_LOG( "Can't create a new connection");
		return cJSON_CreateString("cannot_connect_to_inner");
	}

//	return cJSON_CreateString("opendoor_request_is_done");
}

cJSON * opendoor_request(jrpc_context * ctx, cJSON * params, cJSON *id) {
	
	if(g_current_calling_state.calling_state != IDLE) {
		return cJSON_CreateString("line_is_busy");
	}

        cJSON *item;
        innerbox_info_t *loop;

        if(params!=NULL) {
                item = cJSON_GetObjectItem(params,"doorid");
                loop=g_innerbox_info_list;
                DEBUG_INFO("in params checks");
                if(item != NULL && item->type == cJSON_String){

//temporary for password opendoor
#ifdef YANGLIUJUN
	if (strcmp(item->valuestring,"1509") == 0) {
		send_msg_to_server("127.0.0.1",7890,"{\"method\":\"opendoor_permit\"}");
		send_msg_to_server("127.0.0.1",7890, "{\"method\":\"call_ending_from_local\"}");
		return cJSON_CreateString("password_opendoor_cmd");		
	}
#endif
		loop =  select_from_innerbox_info_list(item->valuestring);
/*
                        while(loop) {
                                if(strcmp(loop->doorid,item->valuestring) == 0)
                                        break;
                                loop = loop->next;
                        }
*/
                }
        }
        else
                return cJSON_CreateString("null_params");

        if(loop == NULL) {
		write_door_id();
                DEBUG_ERR_LOG("innerbox ip not found");
                return cJSON_CreateString("innerbox_ip_not_found");
        }

        DEBUG_INFO("opendoor to:%s",loop->ipaddr);

	create_worker(do_opendoor_request,loop->ipaddr);

	//send_msg_to_server(loop->ipaddr,5678, "{\"method\":\"video_record\"}");
	return cJSON_CreateString("opendoor_request_is_done");
}

void * do_qrcode_opendoor_request()
{
	pthread_detach(pthread_self());
	
	char portstr[16];
        bzero(portstr,16);
	struct ev_loop *loopinthread =ev_loop_new(0);

	//ev_run(loopinthread,0);
	
        snprintf(portstr,16,"%d",PROPERTY_PORT);
        connect_watcher *c = new_connector(loopinthread,g_baseconf.propertyip, portstr);
        if(!c) {
                DEBUG_ERR_LOG("Cannot connect to property server %s:%d",g_baseconf.propertyip,PROPERTY_PORT);
                return cJSON_CreateString("cannot_connect_to_property");
        }
        //memset(&g_connectout_watcher,0,sizeof(struct connect_watcher));
        c->buffer_size = 1500;
        if(!c->buffer)
                c->buffer = malloc(1500);
        memset(c->buffer, 0, 1500);
        c->pos = 0;

        //write command to property 
        c->eio.data = c;
        c->data = g_qrcode_opendoor_request_to_property;

        //int ret =  write(c->eio.fd,call_from_outerbox,strlen(call_from_outerbox));
        if (c) {
                DEBUG_INFO(
                "Trying eonnection to %s:%d...",g_baseconf.propertyip,PROPERTY_PORT
                );

		//pthread_mutex_lock(&lock);
		//g_current_calling_state.calling_state =  CONNECTING;
		//g_current_calling_state.calling_port =  PROPERTY_PORT;
		//strcpy(g_current_calling_state.calling_ipaddr,g_baseconf.propertyip);
		//time_t now = time(&now);
		//g_current_calling_state.connect_start_time=now;
		//g_current_calling_state.calling_socket = c->eio.fd;

                ev_io_start(loopinthread, &c->eio);
		ev_run(loopinthread,0);
		

		//timer
		//ev_timer_init (&g_timeout_watcher, timeout_cb, 30, 0.); // Non repeating timer. The timer starts repeating in the timeout callback function
    		//ev_timer_start (loop_read, &g_timeout_watcher);
        }
        else {
                DEBUG_ERR_LOG( "Can't create a new connection");
 //               return cJSON_CreateString("cannot_connect_to_proerty");
        }

//        return cJSON_CreateString("call_property_is_done");

}

/*qrcode opendoor yangliujun*/
cJSON * qrcode_opendoor_cmd(jrpc_context * ctx, cJSON * params, cJSON *id) {

        cJSON *qrcode;

	bzero(g_qrcode_opendoor_request_to_property,2048);

        if(params!=NULL) {
                qrcode = cJSON_GetObjectItem(params,"qrcode");
        	bzero(g_qrcode_opendoor_request_to_property,2048);
                if(qrcode != NULL && qrcode->type == cJSON_String){
			snprintf(g_qrcode_opendoor_request_to_property,2048,"{\"method\":\"qrcode_opendoor_quest\",\"params\":{\"qrcode\":\"%s\",\"ipaddr\":\"%s\"}}",qrcode->valuestring,g_localaddress);
                }
		else
			return cJSON_CreateString("null_params");
        }
        else
                return cJSON_CreateString("null_params");

	create_worker(do_qrcode_opendoor_request,NULL);

	return cJSON_CreateString("qrcode_opendoor_cmd_get");
}

void * do_bluetooth_opendoor_request()
{
	pthread_detach(pthread_self());
	
	char portstr[16];
        bzero(portstr,16);
	struct ev_loop *loopinthread =ev_loop_new(0);

	//ev_run(loopinthread,0);
	
        snprintf(portstr,16,"%d",PROPERTY_PORT);
        connect_watcher *c = new_connector(loopinthread,g_baseconf.propertyip, portstr);
        if(!c) {
                DEBUG_ERR_LOG("Cannot connect to property server %s:%d",g_baseconf.propertyip,PROPERTY_PORT);
                return cJSON_CreateString("cannot_connect_to_property");
        }
        //memset(&g_connectout_watcher,0,sizeof(struct connect_watcher));
        c->buffer_size = 1500;
        if(!c->buffer)
                c->buffer = malloc(1500);
        memset(c->buffer, 0, 1500);
        c->pos = 0;

        //write command to property 
        c->eio.data = c;
        c->data = g_bluetooth_opendoor_request_to_property;

        //int ret =  write(c->eio.fd,call_from_outerbox,strlen(call_from_outerbox));
        if (c) {
                DEBUG_INFO(
                "Trying connection to %s:%d...",g_baseconf.propertyip,PROPERTY_PORT
                );

		//pthread_mutex_lock(&lock);
		//g_current_calling_state.calling_state =  CONNECTING;
		//g_current_calling_state.calling_port =  PROPERTY_PORT;
		//strcpy(g_current_calling_state.calling_ipaddr,g_baseconf.propertyip);
		//time_t now = time(&now);
		//g_current_calling_state.connect_start_time=now;
		//g_current_calling_state.calling_socket = c->eio.fd;

                ev_io_start(loopinthread, &c->eio);
		ev_run(loopinthread,0);

		//timer
		//ev_timer_init (&g_timeout_watcher, timeout_cb, 30, 0.); // Non repeating timer. The timer starts repeating in the timeout callback function
    		//ev_timer_start (loop_read, &g_timeout_watcher);
        }
        else {
                DEBUG_ERR_LOG( "Can't create a new connection");
 //               return cJSON_CreateString("cannot_connect_to_proerty");
        }

//        return cJSON_CreateString("call_property_is_done");

}

/*bluetooth opendoor yangliujun*/
cJSON * bluetooth_opendoor_cmd(jrpc_context * ctx, cJSON * params, cJSON *id) {

        cJSON *sessionid;

	bzero(g_bluetooth_opendoor_request_to_property,2048);

        if(params!=NULL) {
                sessionid = cJSON_GetObjectItem(params,"sessionid");
        	bzero(g_qrcode_opendoor_request_to_property,2048);
                if(sessionid != NULL && sessionid->type == cJSON_String){
			snprintf(g_bluetooth_opendoor_request_to_property,2048,"{\"method\":\"bluetooth_opendoor_quest\",\"params\":{\"sessionid\":\"%s\",\"ipaddr\":\"%s\"}}",sessionid->valuestring,g_localaddress);
                }
		else
			return cJSON_CreateString("null_params");
        }
        else
                return cJSON_CreateString("null_params");

	create_worker(do_bluetooth_opendoor_request,NULL);

	return cJSON_CreateString("bluetooth_opendoor_cmd_get");
}


/*heartbeat server start*/
#define DEFAULT_PORT    9990
#define BUF_SIZE        4096
int sd; // socket descriptor
struct sockaddr_in addr;
int addr_len = sizeof(addr);
char buffer[BUF_SIZE];

void udp_cb(EV_P_ ev_io *w, int revents) {
    puts("udp socket has become readable");
    socklen_t bytes = recvfrom(sd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*) &addr, (socklen_t *) &addr_len);

    // add a null to terminate the input, as we're going to use it as a string
    buffer[bytes] = '\0';

    DEBUG_INFO("udp client said: %s", buffer);

    // Echo it back.
    // WARNING: this is probably not the right way to do it with libev.
    // Question: should we be setting a callback on sd becomming writable here instead?
    sendto(sd, buffer, bytes, 0, (struct sockaddr*) &addr, sizeof(addr));

}

void * heartbeat_serverinit()
{
	int port = DEFAULT_PORT;
	pthread_detach(pthread_self());
    	puts("udp_echo server started...");

    	// Setup a udp listening socket.
    	sd = socket(PF_INET, SOCK_DGRAM, 0);
    	bzero(&addr, sizeof(addr));
    	addr.sin_family = AF_INET;
    	addr.sin_port = htons(port);
    	addr.sin_addr.s_addr = INADDR_ANY;
    	if (bind(sd, (struct sockaddr*) &addr, sizeof(addr)) != 0)
        	perror("bind");

    	// Do the libev stuff.
    	struct ev_loop *loop1 = ev_loop_new(0);
    	ev_io udp_watcher;
    	ev_io_init(&udp_watcher, udp_cb, sd, EV_READ);
    	ev_io_start(loop1, &udp_watcher);
    	ev_run(loop1, 0);

    	// This point is never reached.
    	close(sd);
}

/*heartbeat server end*/

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

        snprintf(buffer,1024,"{\"method\":\"heartbeat\",\"params\":{\"ipaddr\":\"%s\"}}",g_localaddress);

        sendto(s,buffer,strlen(buffer),0,(struct sockaddr *) &addr, addr_len);
	
	close(s);
}

//using udp
void * heartbeat_to_property()
{
	if(g_ipconf_get_flag == 0){
		DEBUG_INFO("send_msg_to_server");
	//	send_msg_to_server("127.0.0.1",7890, "{\"method\":\"ipconf_updated\"}");
	}

	pthread_detach(pthread_self());

	DEBUG_INFO("in heartbeat");
        //struct ev_loop *loop_tick = ev_default_loop(0);
        struct ev_loop *loop_tick = ev_loop_new(0);
        ev_periodic heartbeat_tick;
        ev_periodic_init(&heartbeat_tick, heartbeat_cb, 0., 60.,0);
        ev_periodic_start(loop_tick, &heartbeat_tick);
        ev_run(loop_tick,0);

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

void query_info_from_property(char *cmdstr, struct ev_loop * loop)
{

	//close(g_connectout_watcher.eio.fd);
	char portstr[16];
	bzero(portstr,16);
	snprintf(portstr,16,"%d",PROPERTY_PORT);
	connect_watcher *c = new_connector(loop,g_baseconf.propertyip, portstr);
	if(!c) {
		DEBUG_ERR_LOG("Cannot connect to property server");
		return;
	}
	//memset(&g_connectout_watcher,0,sizeof(struct connect_watcher));
	c->buffer_size = 1500;
	if(!c->buffer)
		c->buffer = malloc(1500);
	memset(c->buffer, 0, 1500);
	c->pos = 0;

	//write command to property 
 	//send(c->eio.fd,cmdstr,strlen(cmdstr),0);
	DEBUG_INFO("cmdstr:%s",cmdstr);
	c->eio.data = c;
	c->data = cmdstr;

	if (c) {
		DEBUG_INFO( 
		"Trying connection to %s:%d...",g_baseconf.propertyip,PROPERTY_PORT
		);
		ev_io_start(loop, &c->eio);
		ev_run(loop,0);
	}
	else {
	       	DEBUG_ERR_LOG( "Can't create a new connection");
		return;
	}

}

#ifdef BEIJING
unsigned char call_elev_data[FILE_LEN_MAX];
unsigned int  call_elev_len;
unsigned char elev_member[FILE_LEN_MAX];
unsigned int  elev_memb_len;
int           socket_485_server = 0;
char          socket_485_serverIP[128];

int init_readfiles(void)
{
	FILE *fp;
	int  len;
	
	//////////////////////////////////////////////////////////
	//   读取各个房间的呼梯命令参数
	fp = NULL;
	fp = fopen(CALL_ELEV_FILENAME,"rb");
	if(fp==NULL)
	{
		DEBUG_ERR_LOG("open call elevator data file error!!");
		return -1;
	}
	len = getfilelen(fp);
	call_elev_len = len/sizeof(ROOM_ATTRIBUTE);
	memset((unsigned char *)call_elev_data,0,FILE_LEN_MAX);
	fread(call_elev_data,1,len,fp);
	fclose(fp);
	DEBUG_INFO("call_elev_len : %d ",call_elev_len);
	//
	//////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////
	//	  读取网络转485模块的IP地址
	fp = NULL;
	fp = fopen(REMOTE_IP_FILENAME,"rb");
	if(fp==NULL)
	{
		DEBUG_ERR_LOG("open remote 485 server ip file error!!");
		return -1;
	}
	len = getfilelen(fp);
	memset(socket_485_serverIP,0,128);
	fread(socket_485_serverIP,1,len,fp);
	fclose(fp);
	//
	//////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////
	//		读取电梯心跳包的个数
	fp = NULL;
	fp = fopen(ELEV_MEMB_FILENAME,"rb");
	if(fp==NULL)
	{
		DEBUG_ERR_LOG("open call elevator data file error!!");
		return -1;
	}
	len = getfilelen(fp);
	elev_memb_len = len/sizeof(ELEVATOR_MEMBER);
	memset((unsigned char *)elev_member,0,FILE_LEN_MAX);
	fread(elev_member,1,len,fp);
	fclose(fp);
	DEBUG_INFO("elev_memb_len : %d",elev_memb_len);
	//
	//////////////////////////////////////////////////////////
}

int connect_485_server(void)
{
	struct sockaddr_in addr;
	
	socket_485_server=socket(AF_INET,SOCK_STREAM,0);
	if(socket_485_server<0)
	{
		DEBUG_ERR_LOG("socket error: socket_485_server");
		return -1;
	}
	
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(REMOTE_IP_PORT);
	addr.sin_addr.s_addr = inet_addr(socket_485_serverIP);
 	unsigned long ul = 1;
       	ioctl(socket_485_server, FIONBIO, &ul);
	fd_set set;
	struct timeval tm;
	int error = -1, len;
	len = sizeof(int);
	int ret = 0;

	if(connect(socket_485_server,(struct sockaddr *)&addr,sizeof(addr))<0)
	{
		tm.tv_sec = 2;
		tm.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(socket_485_server, &set);
		if(select(socket_485_server+1, NULL, &set, NULL, &tm) > 0) {
			getsockopt(socket_485_server, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
			if(error == 0) ret = 0;
		}

	}


	int keepalive = 1;
	setsockopt(socket_485_server, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
	
	if(ret == -1) {
        	DEBUG_ERR_LOG("connect error: %s(errno: %d)\n",strerror(errno),errno);
        	return ret;
	}

	DEBUG_INFO("connect 485 server right !!!");
	return 0;
}


/*
//   20170719  楼层互访功能
//参数：1.from_room; 2.to_room
//说明: 电梯到from_room 接人,到 to_room
//      这个程序自动判断当前的两个房间号是否使用一部电梯，
//      对上层来说，调用这个接口后返回值是0，则发送梯控命令;返回值不是0，就不发送梯控命令
*/
int send_elev_call_cmd(int from_room, int to_room, unsigned char *outdata)
{
        int i;
        ROOM_ATTRIBUTE *room_attribute=NULL;
        unsigned char send_call[] = {0x02,0x02,0x01,0x06,0x01,0x00,0xB0,0x01,0x00,0x03,0x42,0x03};

        room_attribute = (ROOM_ATTRIBUTE *)call_elev_data;

        //查找接客楼层 
        DEBUG_INFO("-->from_room : %d",from_room);
        for(i=0;i<call_elev_len;i++)
        {
                DEBUG_INFO("room_attribute[%d].room_num : %d",i,room_attribute[i].room_num);
                if(room_attribute[i].room_num == from_room)
                        break;
        }

        if(i>=call_elev_len)
        {
                DEBUG_ERR_LOG("from_room error!");
                return -2;
        }
        else
        {
                send_call[7] = room_attribute[i].plies;
                send_call[1] = room_attribute[i].bnk;
                send_call[2] = room_attribute[i].nod;
        }

        //查找目的楼层  
        DEBUG_INFO("-->to_room : %d",to_room);
        for(i=0;i<call_elev_len;i++)
        {
                DEBUG_INFO("room_attribute[%d].room_num : %d",i,room_attribute[i].room_num);
                if(room_attribute[i].room_num == to_room)
                        break;
        }

        if(i>=call_elev_len)
        {
                DEBUG_ERR_LOG("to_room error!");
                return -3;
        }
        else
        {
                send_call[9] = room_attribute[i].plies;
        }

        //准备呼梯数据
        if((send_call[1] == room_attribute[i].bnk) && (send_call[2] == room_attribute[i].nod))
        {    // 如果两个房间使用同一部电梯
                send_call[10] = get_check_sum(send_call);
                memcpy((unsigned char *)outdata,(unsigned char *)send_call,sizeof(send_call));
                return 0;
        }
        else
        {  // 两个房间用的是不一样的电梯
            return -4;
        }
}


/*
//
//参数：1.room_num; 2.(id=GUEST 访客; id=HOST 主人;)
//说明：1.访客呼梯命令；2.主人呼梯命令
//
*/
int get_elev_call_cmd(int room_num, int id, unsigned char *outdata)
{
	int i;
	ROOM_ATTRIBUTE *room_attribute=NULL;
	unsigned char guest_call[] = {0x02,0x02,0x01,0x06,0x01,0x00,0xB0,0x01,0x00,0x03,0x42,0x03};
	// unsigned char host_call[] = {0x02,0x01,0x01,0x06,0x01,0x00,0x40,0x01,0x10,0xFF,0xA7,0x03};
	//unsigned char host_call[] = {0x02,0x01,0x01,0x06,0x01,0x00,0xB0,0x01,0x10,0x01,0xA7,0x03};
	unsigned char host_call[] = {0x02,0x01,0x01,0x06,0x01,0x00,0xB0,0x01,0x00,0x01,0xA7,0x03};
	
	// if(room_num > PLIIES_MAX)
		// return -1;
	
	room_attribute = (ROOM_ATTRIBUTE *)call_elev_data;
	
	DEBUG_INFO("-->room_num : %d",room_num);
	for(i=0;i<call_elev_len;i++)
	{
		DEBUG_INFO("room_attribute[%d].room_num : %d",i,room_attribute[i].room_num);
		if(room_attribute[i].room_num == room_num)
			break;
	}
	
	if(i>=call_elev_len)
	{
		DEBUG_ERR_LOG("room_num error!");
		return -2;
	}
	else
	{
		if(id == GUEST)
		{
			guest_call[1] = room_attribute[i].bnk;
			guest_call[2] = room_attribute[i].nod;
			guest_call[9] = room_attribute[i].plies;
			guest_call[10] = get_check_sum(guest_call);
			memcpy((unsigned char *)outdata,(unsigned char *)guest_call,sizeof(guest_call));
		}
		else if(id == HOST)
		{
			host_call[1] = room_attribute[i].bnk;
			host_call[2] = room_attribute[i].nod;
			host_call[7] = room_attribute[i].plies;
			host_call[10] = get_check_sum(host_call);
			memcpy((unsigned char *)outdata,(unsigned char *)host_call,sizeof(host_call));
		}
		else
		{
			DEBUG_ERR_LOG("id error");
			return -3;
		}
	}
	
	return 0;
}

unsigned char get_check_sum(unsigned char *indata)
{
	int len;
	int i;
	unsigned char tmp;
	
	tmp = 0;
	len = indata[3]+3;
	
	for(i=0;i<len;i++)
	{
		tmp += indata[i+1];
	}
	
	tmp = (~tmp)+1;
	
	return tmp;
}

long getfilelen(FILE *fp)
{
	  long curpos, length; 
	  curpos = ftell(fp);
	  fseek(fp, 0, SEEK_END);
	  length = ftell(fp); 
	  fseek(fp, curpos, SEEK_SET);
	  if(length > FILE_LEN_MAX)
		return 0;
	  else
		return length;
}

#endif

cJSON * call_elevator(jrpc_context * ctx, cJSON * params, cJSON *id) {
	char innerbox_ip[128];
	char innerbox_ip_from[128];
	int roomid = 0;
	int roomid_from = 0;
	int ret = 0;
	cJSON* cmdobj;
	cJSON* ipobj;
	cJSON* ipobj_from;
	int cmd = 0;

	bzero(innerbox_ip,128);
	bzero(innerbox_ip_from,128);
	
	if(params != NULL && params->type == cJSON_Object) {
		cmdobj = cJSON_GetObjectItem(params,"cmd");
		if(cmdobj != NULL && cmdobj->type == cJSON_Number){
			cmd = cmdobj->valueint;			
		}

		ipobj = cJSON_GetObjectItem(params,"ipaddr");
		if(ipobj != NULL && ipobj->type == cJSON_String){
			strcpy(innerbox_ip,ipobj->valuestring);		
		}

		ipobj_from = cJSON_GetObjectItem(params,"ipaddr_from");
		if(ipobj_from != NULL && ipobj_from->type == cJSON_String){
			strcpy(innerbox_ip_from,ipobj_from->valuestring);		
		}
		
	}

	//travel_innerbox_infolist();

	innerbox_info_t *loop=g_innerbox_info_list;
	DEBUG_INFO("in params checks");
	while(loop) {
		if(strcmp(loop->ipaddr,innerbox_ip) == 0) {
			roomid = atoi(loop->doorid);
			break;
		}
		loop = loop->next;
	}		
	
	loop=g_innerbox_info_list;
	DEBUG_INFO("in params checks");
	while(loop) {
		if(strcmp(loop->ipaddr,innerbox_ip_from) == 0) {
			roomid_from = atoi(loop->doorid);
			break;
		}
		loop = loop->next;
	}		

#ifdef BEIJING
	char outbuf[128];
	char inbuf[256];
	int recv_count = 0;
	int i = 0;

	bzero(outbuf,128);

	init_readfiles();
	char peeraddress[128];
	if(get_peer_address(socket_485_server,peeraddress)!=9527) {	
		ret = connect_485_server();
		if(ret != 0) {
			DEBUG_ERR_LOG("connect 485 TCP server error !");
			return cJSON_CreateString("cannot_connect_to_485");
		}
	}

	if(cmd == 1 || cmd == 2)
		ret = get_elev_call_cmd(roomid, cmd,outbuf);
	if(cmd == 3)
		ret = send_elev_call_cmd(roomid_from, roomid, outbuf);

	if(ret != 0) {
		DEBUG_ERR_LOG("get_elev_call_cmd error !");
		return cJSON_CreateString("get_elev_call_cmd_error");
	}
		
	DEBUG_INFO("\n<------------------------>\n");
	for(i=0;i<(outbuf[3]+6);i++) {
		DEBUG_INFO("0x%02x ",outbuf[i]);
	}
	DEBUG_INFO("\n<------------------------>\n");
		
	usleep(384000);
	//发送指令到转发器
	ret = send(socket_485_server,outbuf,(outbuf[3]+6),0);
	if(ret < 0)
	{
		DEBUG_ERR_LOG("send data to 485 server error");
		return cJSON_CreateString("send data to 485 server error");
	}

	usleep(100000);
	//接收一次转发器返回的应答
	recv_count = recv(socket_485_server,inbuf,sizeof(inbuf),0);
	if(recv_count<=0)
	{
		DEBUG_ERR_LOG("recv 485 server socket error!");
		return cJSON_CreateString("recv 485 server socket error!");
	}
/*
	usleep(384000);
	
	ret = send(socket_485_server,outbuf,(outbuf[3]+6),0);
        if(ret < 0)
        {
                printf("send data to 485 server error\n");
                return cJSON_CreateString("send data to 485 server error");
        }

        usleep(10);
        //接收一次转发器返回的应答
        recv_count = recv(socket_485_server,inbuf,sizeof(inbuf),0);
        if(recv_count<=0)
        {
                printf("recv 485 server socket error!\n");
                return cJSON_CreateString("recv 485 server socket error!");
        }
*/
	//close(socket_485_server);

#endif

	return cJSON_CreateString("call_elevator_cmd_send");
}

cJSON * show_version(jrpc_context * ctx, cJSON * params, cJSON *id) {
        return cJSON_CreateString(g_baseconf.version);
}

cJSON * softupdate_query(jrpc_context * ctx, cJSON * params, cJSON *id) {

	char cmdstr[1024];
	snprintf(cmdstr,1024,"{\"method\":\"softupdate_query\",\"params\":{\"version\":\"%s\",\"ipaddr\":\"%s\"}}",g_baseconf.version,g_localaddress);
	send_msg_to_server(g_baseconf.propertyip,18699, cmdstr);
	
	return cJSON_CreateString(g_baseconf.version);
}

void * do_ipaddr_query_from_property()
{
	struct ev_loop * loop;
//	char cmdstr[2048];	

	loop = ev_loop_new(0);
	if (!loop) {
		DEBUG_ERR_LOG("Can't initialise libev; bad $LIBEV_FLAGS in environment?");
		exit(1);
	}

//	bzero(cmdstr,2048);
//	snprintf(cmdstr,2048,"{\"method\":\"ipaddr_query_of_innerbox\",\"params\":{\"ipaddr\":\"%s\"}}",g_localaddress);
	
	query_info_from_property(g_ipquery_cmdstr,loop);
	//ev_run(loop, 0);
}

void * ipaddr_query_from_property()
{
	pthread_detach(pthread_self());
	sleep(20);
	while(g_ipconf_get_flag == 0){
		do_ipaddr_query_from_property();
		sleep(30);
	}
}

void statcheck_cb(struct ev_loop *loop, ev_periodic *w, int revents)
{
	time_t now = time(&now);

	DEBUG_INFO("in state_check:%d %d %d",(int)now,(int)g_current_calling_state.connect_start_time,(int)g_current_calling_state.call_start_time);
	if(g_current_calling_state.calling_state == CONNECTING) {
		if((now-g_current_calling_state.connect_start_time)>=30){
			DEBUG_INFO("timeout of connecting");
			create_worker(do_call_ending,0);
		}
	}

	if(g_current_calling_state.calling_state == CALLING) {
		if((now-g_current_calling_state.call_start_time)>=60*2){
			DEBUG_INFO("timeout of calling");
			create_worker(do_call_ending,0);
		}
	}

	innerbox_info_t * list = g_innerbox_info_list;
	int count = 0;
	while(list) {
		count++;
		list = list->next;
	}

	if(count == 0 || g_innerbox_count == 0 ) {
		
		g_ipconf_get_flag = 0;
	}
 
}

void * state_check()
{
	DEBUG_INFO("in state_check");
	pthread_detach(pthread_self());
        //struct ev_loop *loop_tick = ev_default_loop(0);
        struct ev_loop *loop_tick = ev_loop_new(0);
        ev_periodic heartbeat_tick;
        ev_periodic_init(&heartbeat_tick, statcheck_cb, 0., 10.,0);
        ev_periodic_start(loop_tick, &heartbeat_tick);
        ev_run(loop_tick,0);
}


struct ethtool_value {
        __uint32_t      cmd;
        __uint32_t      data;
};

int eth_stat()
{
	struct ethtool_value edata;
    	int fd = -1, err = 0;
    	struct ifreq ifr;

        memset(&ifr, 0, sizeof(ifr));
        strcpy(ifr.ifr_name, "eth0");
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
                perror("Cannot get control socket");
                return -1;
        }

        edata.cmd = 0x0000000a;
        ifr.ifr_data = (caddr_t)&edata;
        err = ioctl(fd, 0x8946, &ifr);
        if (err == 0) {
		close(fd);
                fprintf(stdout, "Link detected: %s\n",
                        edata.data ? "yes":"no");
		return edata.data;
        } else if (errno != EOPNOTSUPP) {
                perror("Cannot get link status");
        }
	close(fd);
   	return 0;
}

void * sys_stat_watch()
{
	pthread_detach(pthread_self());

	int i= 0;
	unsigned char xor = 0;
	unsigned char sys_stat[8];
	unsigned char last_stat = 3;
	while(1) {
		if(eth_stat()==1) {
			g_sys_stat |= STAT_NET_UP;	
		}else{
			g_sys_stat &= ~STAT_NET_UP;
		}
		DEBUG_INFO("sys stat:%d",g_sys_stat);
#ifdef YANGLIUJUN
		bzero(sys_stat,8);
		strcpy(sys_stat, "\xaa\x06\x31\x03\x11\x55");
		sys_stat[3] = g_sys_stat;
                xor = 0;
                for(i=0;i<4;i++) {
                        xor^=sys_stat[i];
                }
		sys_stat[4] = xor;
		if(last_stat != g_sys_stat);
			write_uart_cmd(sys_stat,6);
		last_stat = g_sys_stat;
#endif	
		sleep(1);	
	}
}

void * server_watchdog()
{

        pthread_detach(pthread_self());
        int pid = getpid();
	char cmd[256];
	bzero(cmd,256);
	sprintf(cmd,"ls -l /proc/%d/fd|wc -l",pid);	
	char result[1024];
	
        while(1) {
                sleep(30);
		bzero(result,1024);
		execute_cmd(cmd,result);
			
		if(g_current_calling_state.calling_state == CALLING)
			continue;
                if(atoi(result) > 1000 || send_msg_to_server("127.0.0.1",7890,"{\"method\":\"show_version\"}") ==0 ) {
                        DEBUG_ERR_LOG("service error & restart ");
                        kill(pid,9);
                }
        }
}


int main(void) {
	int pid= getpid();
	FILE * file = fopen(PID_PATH,"w");
	if(file){
		fprintf(file,"%d",pid);
		fclose(file);
	}

	bzero(g_stream_outerbox,1024);
	bzero(g_localaddress,64);
	bzero(&g_current_calling_state,sizeof(g_current_calling_state));
//	memset(&g_connectout_watcher,0,sizeof(struct connect_watcher));

	bzero(g_call_from_outerbox,2048);
	bzero(g_opendoor_request,256);

  	signal(SIGPIPE,SIG_IGN);

	get_local_address();
//	snprintf(g_call_from_outerbox,2048,"{\"method\":\"call_from_outerbox\",\"params\":{\"rtspurl\":\"%s\",\"ipaddr\":\"%s\"}}",g_stream_outerbox,g_localaddress);
	snprintf(g_opendoor_request,256,"{\"method\":\"opendoor\",\"params\":{\"ipaddr\":\"%s\"}}",g_localaddress);
	
	bzero(g_ipquery_cmdstr,2048);
	snprintf(g_ipquery_cmdstr,2048,"{\"method\":\"ipaddr_query_of_innerbox\",\"params\":{\"ipaddr\":\"%s\"}}",g_localaddress);

	init_stream_url();
	get_basic_conf();
	DEBUG_INFO("version:%s",g_baseconf.version);
	read_innerbox_info_from_conf();
	//travel_innerbox_infolist();
	create_basic_conf_file();

	create_worker(ipaddr_query_from_property,0);
//	create_worker(heartbeat_to_property,0);
/*888888888*/
	create_worker(heartbeat_serverinit,0);
	create_worker(state_check,0);
	create_worker(sys_stat_watch,0);
	create_worker(server_watchdog,0);

	jrpc_server_init(&my_out_server, OUT_PORT);
	jrpc_register_procedure(&my_out_server, opendoor_permit, "opendoor_permit", NULL );
	jrpc_register_procedure(&my_out_server, opendoor_permit, "opendoor", NULL );
	jrpc_register_procedure(&my_out_server, opendoor_request, "opendoor_request", NULL );
	jrpc_register_procedure(&my_out_server, qrcode_opendoor_cmd, "qrcode_opendoor_cmd", NULL );
	jrpc_register_procedure(&my_out_server, bluetooth_opendoor_cmd, "bluetooth_opendoor_cmd", NULL );
	jrpc_register_procedure(&my_out_server, gate_opendoor_request, "gate_opendoor_request", NULL );
	jrpc_register_procedure(&my_out_server, opendoor_deny, "opendoor_deny", NULL );
	jrpc_register_procedure(&my_out_server, play_live_audio, "play_live_audio", NULL );
	jrpc_register_procedure(&my_out_server, calling_state, "calling_state", NULL );
	jrpc_register_procedure(&my_out_server, call_property, "call_property", NULL );
	jrpc_register_procedure(&my_out_server, call_ending, "call_ending", NULL );
	jrpc_register_procedure(&my_out_server, call_ending_from_local, "call_ending_from_local", NULL );
	jrpc_register_procedure(&my_out_server, call_elevator, "call_elevator", NULL );
	jrpc_register_procedure(&my_out_server, ipconf_updated, "ipconf_updated", NULL );
	jrpc_register_procedure(&my_out_server, softupdate_outerbox, "softupdate_outerbox", NULL );
	jrpc_register_procedure(&my_out_server, show_version, "show_version", NULL );
	jrpc_register_procedure(&my_out_server, softupdate_query, "softupdate_query", NULL );
//	heartbeat_serverinit();
//	heartbeat_to_property();
	jrpc_server_run(&my_out_server);
	jrpc_server_destroy(&my_out_server);
 	return 0;
}
