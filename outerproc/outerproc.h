#ifndef OUTERPROC_H_
#define OUTERPROC_H_

#define MAX_FD_NUMBER 100
int connection_fd_array[MAX_FD_NUMBER]; //max 100 connections on one time
#define OUT_PORT 7890
#define HEARTBEAT_PORT 9990

#define DEBUG_LOG_FILE "/var/log/outer_err.log"

cJSON * opendoorcmd_to_app();
cJSON * msgpush_to_app();

//sentrybox struct
typedef struct sentrybox_conf {
        char name[64];
        char ipaddr[64];
} sentrybox_conf_t;

typedef struct basic_conf {
        char version[64];
        char outerinterfaceip[64];
        char propertyip[64];
        char callcenterip[64];
        char dcip[64];
        unsigned int dcserial;
        unsigned int dcid;
        int sentryboxcount;
        sentrybox_conf_t sentrybox[100];
} basic_conf_t;

typedef struct innerbox_info {
	char doorid[128];
	char ipaddr[128];
	struct innerbox_info * next;
} innerbox_info_t;

void get_time_now(char * datetime) {
        time_t now;
        struct tm *tm_now;
       // char    datetime[200];

        time(&now);
        tm_now = localtime(&now);
        strftime(datetime, 200, "%Y-%m-%d %H:%M:%S", tm_now);

}

#ifndef __USE_DEBUG
#define __USE_DEBUG

#define USE_DEBUG
#ifdef USE_DEBUG
#define DEBUG_LINE() printf("[%s:%s] line=%d\r\n",__FILE__, __func__, __LINE__)
#define DEBUG_ERR(fmt, args...) printf("[%s:%d] "#fmt" errno=%d, %m\r\n", __func__, __LINE__, ##args, errno, errno)
#define DEBUG_ERR_LOG(fmt, args...) \
	do {\
		FILE* fd = fopen(DEBUG_LOG_FILE,"a");\
		if(fd) {\
			char timestr[64];\
			bzero(timestr,64);\
			get_time_now(timestr);\
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
