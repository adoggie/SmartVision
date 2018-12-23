#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "md5sum.h"
#include <sys/ioctl.h>
#include <linux/watchdog.h>

#define handle_error(msg) \
	           do { perror(msg); exit(EXIT_FAILURE); } while (0)

#ifdef YANGCHUANG
#include "ycapic.h"
#endif

#ifdef MAICHONG
int watchdog_close(){
	int  watchdogfd = open("/dev/watchdog", O_RDWR);
        if (watchdogfd < 0) {
                printf("Unable to open /dev/watchdog\n");
		return -1;
        }

        int flags = WDIOS_DISABLECARD;
        int result = ioctl(watchdogfd, WDIOC_SETOPTIONS, &flags);
        if (result < 0) {
                 printf("Unable to stop watchdog\n");
		return -1;
        }
        close(watchdogfd);
        return result;
}
#endif


/* 保证升级过程的可靠性*/
int main (int argc, char *argv[])
{
	
	/*read md5 from md5sum.txt*/

        FILE *stream;
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
	char * temp =NULL;
	int i = 0;
	char chksum[64];
	int sc = 0;
	char name[256];
	char orig[64];
	int retry = 0;
/*we should stop watchdog before do upgrade*/
#ifdef YANGCHUANG
//	StopWDog();
	system("cp -f /system/bin/innerproc.yc /system/bin/innerproc.yc.bk");
#endif

#ifdef MAICHONG
	system("cp -f /system/xbin/innerproc.mc /system/xbin/innerproc.mc.bk");
//	watchdog_close();	
#endif

#ifdef ANDROID
	system("/data/local/tmp/docopy.sh");
#endif
	system("sync");	

wait:
	i = 0;
	sc = 0;

        stream = fopen("md5sum.txt", "r");
        if (stream == NULL)
             exit(EXIT_FAILURE);

        while ((read = getline(&line, &len, stream)) != -1) {
        //       printf("Retrieved line of length %zu :\n", read);
		temp = strtok(line,"\t \n");
		bzero(orig,64);
		strcpy(orig,temp);
		i++;
		if(temp) {
			bzero(name,256);
			temp = strtok(NULL,"\t \n");
			strcpy(name,temp);
			
			bzero(chksum,64);
			if(md5sum(name,chksum) < 0)
				goto err;
			
			if(strcmp(orig,chksum) == 0)
				sc++;
		}	
        }

       	free(line);
	line = NULL;
        fclose(stream);
	//all files seems wright
	//reboot now
	if((i==sc) && (i>0)) {
		system("rm -rf /data/local/tmp/*");	
		system("reboot");
	}
	else {
		system("sync");
		sleep(1);
		if(retry++>30)
			goto err;	
		goto wait;
	}

        exit(EXIT_SUCCESS);

err:
#ifdef MAICHONG
	system("cp -f /system/xbin/innerproc.old /system/xbin/innerproc");
	system("chmod 544 /system/xbin/innerproc");
	system("sync /system/xbin/innerproc");
#endif
#ifdef YANGCHUANG
	system("cp -f /system/bin/innerproc.old /system/bin/innerproc");
	system("chmod 544 /system/bin/innerproc");
	system("sync /system/bin/innerproc");
#endif
	system("rm -rf /data/local/tmp/*");	

	printf("upgrade failure\n");
        exit(EXIT_FAILURE);
	
	return 0;
}

