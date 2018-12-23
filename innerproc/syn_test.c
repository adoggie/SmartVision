#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

#define PORT 80

int main()
{
    char ipaddr[20];
    strcpy(ipaddr,"127.0.0.1");
    int sockfd;
    struct sockaddr_in serv_addr;
    
    FILE *fp;
    fp = fopen("test.txt","w+");
    if(fp == NULL)
    {
        fprintf(fp,"打开文件错误！\n");
        return 0;
    }
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(fp, "socket creating error!\n");
        return 0;
    }
    else
        printf("socket creating success!\n");
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(ipaddr);
    
    int c = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));


    if(c == -1)
    {
        fprintf(fp,"%s : SYN探测包发送成功，等待回应...\n\n",ipaddr);
        if(errno == 111)
            fprintf(fp,"%s : Host is alive!%d",ipaddr,errno);
        else if(errno == 113)
            fprintf(fp,"%s : No route to host!%d",ipaddr,errno);
        else
        {
          if(errno == 146)
            fprintf(fp,"%s : Host is alive!%d",ipaddr,errno);
          else if(errno == 148)
            fprintf(fp,"%s : No route to host!",ipaddr);
          else
            fprintf(fp,"Host is alive!但有错误值返回 : %d",errno);
        }
    }
    
    else if(c == 0)
    {
        fprintf(fp,"%s : SYN探测包发送成功，等待回应...\n\n",ipaddr);
        fprintf(fp,"%s : Host is alive!%d\n",ipaddr,errno);
    }
    
    else
    {
        fprintf(fp,"%s : SYN探测包发送失败！\n\n",ipaddr);
        fprintf(fp,"Connect error ID : %d",c);
    }


    fclose(fp);
    close(sockfd);
    perror("test");

    return 0;
}
