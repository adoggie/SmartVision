#ifndef RC4_H
#define RC4_H

#define SIZE_S 256
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

typedef unsigned char u_char;

typedef struct _rc4_state{
  
    u_char perm[SIZE_S];
  
    u_char index1;
  
    u_char index2;
  
} rc4_state;


void rc4_init(rc4_state *state, u_char *key, int klen);

void rc4_crypt(rc4_state *state,
               const u_char *in, u_char *out, int blen);

int rc4_crypt_file(char *infile,char *outfile);

#endif //RC4_H
