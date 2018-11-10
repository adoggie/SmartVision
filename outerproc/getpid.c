#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
void main() { 
char pidline[1024]; 
char *pid; 
int i =0; 
int pidno[64]; 
FILE *fp = popen("pidof bash","r"); 
fgets(pidline,1024,fp); 
printf("%s",pidline); 
pid = strtok (pidline," "); 
while(pid != NULL) 
{ pidno[i] = atoi(pid); 
printf("%d\n",pidno[i]); 
pid = strtok (NULL , " "); 
i++; 
} 
pclose(fp); 
}
