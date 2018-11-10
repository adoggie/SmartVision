#ifndef OUTERPROC_H_
#define OUTERPROC_H_

#define MAX_FD_NUMBER 100
int connection_fd_array[MAX_FD_NUMBER]; //max 100 connections on one time

typedef enum app_state {
	OFFLINE,
	ONLINE,
	JOINED,
	IDLE,
	CONNECTING,
	CALLING
} app_state_e;

typedef enum call_type {
	ZERO,
	APP2PROPERTY,
	APP2HEALTH,
	PROPERTY2APP,
	HEALTH2APP,
	OUTER2APP,
	APP2OUTER,
	APP2APP
} call_type_e;

typedef struct call_line_state {
	int peersocketfd;
	char peeripaddr[128];
	int peerport;
	call_type_e calltype;
} call_line_state_t;

//app client struct
typedef struct app_client {
        char app_dev_id[512];
	char audiostream_url[1024];
	char audiostream_out[1024];
	char streamserver_forapp[1024];
	char streamserver_forout[1024];
        int socket_fd;
        char ip_addr_str[64];
	char callout_ipaddr[64];
        int port;
        int calling_property;
	int calling_otherinner;
	char calling_otherdoorid[64];
	char calling_otherinner_ip[64];

//        int online;             // app is online or not 1 for on line 0 for not
//        int joined;             // app is joined the family or not 1 for joined 0 for not
        char name[64];          //who use the app
        char type[16];          //phone or pad  
	app_state_e current_state;
        char version[32];       //app client software version
	int state_count;
	call_line_state_t *line_state;
        struct app_client * next;

} app_client_t;

//sentrybox struct
typedef struct sentrybox_conf {
	char name[64];
	char ipaddr[64];
} sentrybox_conf_t;

typedef struct basic_conf {
        char version[64];
        char outerboxip[64];//
        char gateouterboxip[64];//
        char outerboxip_1[64];// 
        char outerinterfaceip[64];
        char familyip[64];
        char propertyip[64];
	char streamserverip[64];
	char callcenterip[64];
	char alarmcenterip[64];
	char doorid[64];
	int sentryboxcount;
	sentrybox_conf_t sentrybox[100];	
} basic_conf_t;

typedef struct seczone_conf {
        int port;
        char name[128];
        char normalstate[16];
        char onekeyset[8];
        char currentstate[8];
        int delaytime;
	int delaycount;
        int nursetime;
        char alltime[8];
        int triggertype;
        char online[8];
	time_t etime;
//      struct seczone_conf *next;
} seczone_conf_t;

typedef struct innerbox_info {
        char doorid[128];
        char ipaddr[128];
        struct innerbox_info * next;
} innerbox_info_t;


#define IN_PORT 18699
#define OUT_PORT 5678
#define RTSP_PORT 554
#define HEARTBEAT_PORT 9990
#define PROPERTY_PORT 18699

#ifdef MAICHONG
#define DEV_GPIO "/dev/sunxi_gpio"
#define GPIO_IOC_MAGIC   'G'
#define IOCTL_GPIO_SETOUTPUT  _IOW(GPIO_IOC_MAGIC, 0, int)       
#define IOCTL_GPIO_SETINPUT  _IOW(GPIO_IOC_MAGIC, 1, int)
#define IOCTL_GPIO_SETVALUE  _IOW(GPIO_IOC_MAGIC, 2, int) 
#define IOCTL_GPIO_GETVALUE  _IOR(GPIO_IOC_MAGIC, 3, int)
#define GPIO_TO_PIN(bank, gpio) (32 * (bank-'A') + (gpio))

typedef struct {        
	int pin;        
	int data;
}am335x_gpio_arg;

#endif


#ifdef ANDROID
#define DEBUG_LOG_FILE "/data/local/tmp/inner_err.log"
#else
#define DEBUG_LOG_FILE "/var/log/inner_err.log"
#endif

#ifndef __USE_DEBUG
#define __USE_DEBUG

#define USE_DEBUG
#ifdef USE_DEBUG

void get_time_log(char * datetime) {
        time_t now;
        struct tm *tm_now;
       // char    datetime[200];

        time(&now);
        tm_now = localtime(&now);
        strftime(datetime, 200, "%Y-%m-%d %H:%M:%S", tm_now);

}


#define DEBUG_LINE() printf("[%s:%s] line=%d\r\n",__FILE__, __func__, __LINE__)
#define DEBUG_ERR(fmt, args...) printf("[%s:%d] "#fmt" errno=%d, %m\r\n", __func__, __LINE__, ##args, errno, errno)
#define DEBUG_ERR_LOG(fmt, args...) \
        do {\
                FILE* fd = fopen(DEBUG_LOG_FILE,"a");\
                if(fd) {\
                        char timestr[64];\
                        bzero(timestr,64);\
                        get_time_log(timestr);\
                        fprintf(fd,"%s [%s:%d] "#fmt" errno=%d, %m\r\n",timestr,__func__,__LINE__,##args, errno,errno);\
                        fclose(fd);\
                }\
                printf("[%s:%d] "#fmt" errno=%d, %m\r\n", __func__, __LINE__, ##args, errno, errno);\
        } while(0)
//#define DEBUG_INFO(fmt, args...) printf("\033[33m[%s:%d]\033[0m "#fmt"\r\n", __func__, __LINE__, ##args)
#define DEBUG_INFO(fmt, args...) printf("[%s:%d] "#fmt"\r\n", __func__, __LINE__, ##args)
#else
#define DEBUG_LINE()
#define DEBUG_ERR(fmt,...)
#define DEBUG_ERR_LOG(fmt, ...) 
#define DEBUG_INFO(fmt,...)
#endif

#endif

#endif
