#include <stdio.h>
#include <stdlib.h>

void main()
{
	FILE* fp;
	int pid_to_be_check=0;

	while( 1 )
	{
       	    sleep(1);
	    if (!(fp=fopen("/etc/serverpid","r")))
	    {
		continue;	
	    }

	   pid_to_be_check = 0;
	   fscanf(fp,"%d",&pid_to_be_check);
	   fclose(fp);
	//   printf("pid:%d\n",pid_to_be_check);
       	   if( kill(pid_to_be_check, 0) < 0 ) system("innerproc &");
	}
}
