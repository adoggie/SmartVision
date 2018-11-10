#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <linux/ioctl.h>
#include <linux/watchdog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <netdb.h>
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
#include <dlfcn.h>
#include <fcntl.h>

void get_time_now(char * datetime) {
        time_t now;
        struct tm *tm_now;
       // char    datetime[200];

        time(&now);
        tm_now = localtime(&now);
        strftime(datetime, 200, "%Y-%m-%d %H:%M:%S", tm_now);

}

#define DEBUG_LOG_FILE "/var/log/dameon.log"

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
#define DEBUG_INFO(fmt,...)
#endif

#endif


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
                DEBUG_ERR_LOG("inet_pton error for %s ",serveraddr);
                return 0;
        }

        unsigned long ul = 1;
        ioctl(sockfd, FIONBIO, &ul);
        int ret = -1;
        fd_set set;
        struct timeval tm;
        int error = -1, len;
        len = sizeof(int);
        printf("connect and send to %s %s\n",serveraddr,cmd);
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
               return 0;
        }

        ul = 0;
        ioctl(sockfd, FIONBIO, &ul);

        //gets(sendline, 4096, stdin);

        if( send(sockfd, cmd, strlen(cmd), 0) < 0)
        {
                DEBUG_ERR_LOG("send msg error");
                return 0;
        }
	usleep(100000);	
	ret = recv(sockfd,recvline,sizeof(recvline),0);
	if(ret <= 0) {
		close(sockfd);
		return 0;
	}
        close(sockfd);
        return 1;
}

int get_procname_bypid(int pid,char* procname)
{
	char cmd[255] = {'\0'};
	FILE *fp = NULL;

	sprintf(cmd,"readlink /proc/%d/exe",pid);
	if((fp = popen(cmd,"r")) != NULL)
	{
	
		if(fgets(procname,255,fp) != NULL) {
			pclose(fp);
			return 1;
		}
	}

	pclose(fp);
	return 0;
}

void main()
{
	FILE* fp;
	int pid_to_be_check=0;
	int count = 0;
	time_t now0,now1;
	int sendcount = 0;
	char procname[255];
	int checkflag = 0;
	char *cmd="{\"method\":\"show_version\"}";
/*
	char pidline[1024];
	
	char *pid;
        int i = 0;
        int pidno[64];

	while(1){
		memset(pidline,0,1024);
		
        	fp = popen("pidof getpid","r");
		fgets(pidline,1024,fp);

		pid = strtok(pidline," ");
		if (pid == NULL)
			system("/home/root/outerproc &");
		else
			continue;
	}
*/

	while( 1 )
	{
       	    	sleep(5);
	    	if (!(fp=fopen("/etc/serverpid","r")))
	    	{
			continue;	
	    	}

	   	pid_to_be_check = 0;
	   	fscanf(fp,"%d",&pid_to_be_check);
	   	fclose(fp);

	   	bzero(procname,255);
	   	get_procname_bypid(pid_to_be_check,procname);
	   	if((strncmp("/home/root/outerproc",procname,20) != 0) || send_msg_to_server("127.0.0.1",7890, cmd) == 0)  {
			DEBUG_INFO("procname:%s",procname);	
	//   printf("pid:%d\n",pid_to_be_check);
       	   	//if( kill(pid_to_be_check, 0) < 0 ) {
			time(&now1);
			
			count++;	
			if(now1-now0<30 && count>2) {

				//system("reboot");
				DEBUG_ERR_LOG("outerproc in error state & restarting\n");
				int fd = open("/dev/watchdog",O_WRONLY);
				int data = 1;
				ioctl (fd, WDIOC_SETTIMEOUT, &data);
				ioctl (fd, WDIOC_GETTIMEOUT, &data);
				while(1)
					sleep(2);
			//	system("reboot");	
				
			}
			else {
				DEBUG_ERR_LOG("error now not restarting %d %d\n",now1-now0, count);
				system("/home/root/outerproc&");	
				now0 = now1;	
			}
		}

	    	if (!(fp=fopen("/etc/touchkey.pid","r")))
	    	{
			continue;	
	    	}

	   	pid_to_be_check = 0;
	   	fscanf(fp,"%d",&pid_to_be_check);
	   	fclose(fp);

	   	bzero(procname,255);
	   	get_procname_bypid(pid_to_be_check,procname);
	   	if((strncmp("/home/root/touchkey",procname,19) != 0))  {

			system("/home/root/touchkey &");
		}

	}

}
