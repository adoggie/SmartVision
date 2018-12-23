#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
 
#include <sys/un.h> 
#include <linux/types.h> 
#include <linux/netlink.h> 
#include  <linux/if.h>
#include  <linux/sockios.h>
#include  <linux/ethtool.h>

#define _UINT32 uint32_t
#define TRUE 1
#define FALSE 0
/*IP地址是否合法, 合法返回TURE，失败返回FALSE*/
int netip_is_valid(char * ipaddr)
{
    int i;
    
    i = inet_addr(ipaddr);
 
    if((i == 0)||(i == 0xffffffff))
        return FALSE;
    else
        return TRUE;
}
   
/*MASK子网掩码是否合法, 合法返回TURE，失败返回FALSE*/
int netmask_is_valid(char * mask)
{
    int i;
    unsigned long ii;
    i = netip_is_valid(mask);
    if(i==TRUE)
    {
        ii = inet_network(mask);
	if((ii == 0) || (ii=0xffffffff))
		return FALSE;
        if((ii|ii-1)==0xffffffff)
        {
            return TRUE;
        }

    }
    
    return FALSE;
}
 
/*MASK子网掩码是否合法, 合法返回TURE，失败返回FALSE*/
int netmask_and_ip_is_valid(char * ip, char * mask)
{
    int i;
    int a, b=0, c;
    i = netip_is_valid(ip);
    if(i!=TRUE)
        return FALSE;
    i = netmask_is_valid(mask);
    if(i!=TRUE)
        return FALSE;
 
    a = inet_network(ip)&0x000000ff;
    b = inet_network(mask);
 
    /*首先与默认子网掩码比较*/
    if(a>0&&a<127)
    {
        if(inet_addr(mask)<0x000000ff)
            return FALSE;
        if(inet_addr(mask)>0x000000ff)
            b-=0xff000000;
    }
    if(a>=128&&a<=191)
    {
        if(inet_addr(mask)<0x0000ffff)
            return FALSE;
        if(inet_addr(mask)>0x0000ffff)
            b-=0xffff0000;
    }
    if(a>=192&&a<=223)
    {
        if(inet_addr(mask)<0x00ffffff)
            return FALSE;
        if(inet_addr(mask)>0x00ffffff)
            b-=0xffffff00;
    }
 
    /*每个子网段的第一个是网络地址,用来标志这个网络,最后一个是广播地址,用来代表这个网络上的所有主机.这两个IP地址被TCP/IP保留,不可分配给主机使用.*/
    c = ~inet_network(mask)&inet_network(ip);
    if(c==0||c==~inet_network(mask))
        return FALSE;
 
    /*RFC 1009中规定划分子网时，子网号不能全为0或1，会导致IP地址的二义性*/
    if(b>0)
    {
        c = b&(inet_network(ip));
        if(c==0||c==b)
            return FALSE;
    }
 
    return TRUE;
}
 
/*测试主网和子网是否匹配，也可测试两个主机IP是否在同一网段内*/
/*
int netIPAndSubnetValid(_UINT32 IP, _UINT32 subIP, _UINT32 mask)
{
    int i;
    int addr1, addr2;
    i = netMaskAndIpIsValid(IP, mask);
    if(i!=TRUE)
    	return FALSE;
    i = netMaskAndIpIsValid(subIP, mask);
    if(i!=TRUE)
    	return FALSE;
 
    addr1 = IP&mask;
    addr2 = subIP&mask;
 
    if(addr1!=addr2)
    	return FALSE;
 
    return TRUE;
}
*/
