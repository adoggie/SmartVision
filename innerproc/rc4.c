/******************************************************************************
* File: rc4.c
******************************************************************************/
#include "rc4.h"
/*
 * This is just an original Whistle Communications implementation of RC4 stream 
 * ciphern with some comments
 *
 * Original source:
 * http://opensource.apple.com//source/xnu/xnu-1456.1.26/bsd/crypto/rc4/rc4.c 
 * 
 * 
 *
 * We don't use mod 256 operations
 * because unsigned char maximum value is 2^8 - 1 = 255
 *
 * For instance:
 *  u_char j;
 *  j += state.perm[i] + key[i % klen] and
 *  j = ( j + state.perm[i] + key[ i % klen] ) mod 256
 *
 *  are the same because unsigned char's maximum value is
 *  2^8 -1 and we will get value between 0 - 255 always
 *  because of that .
 *
 * * */


void swap(u_char *i, u_char *j)
{
   u_char buf;

   buf = *i;
   *i = *j;
   *j = buf;

}


// initialize state
void rc4_init(rc4_state *state, u_char *key, int klen)
{
   u_char j;
   int i;
   //init state with permutation
   for (i = 0; i < SIZE_S; i++)
      state->perm[i] = (u_char) i;


   state->index1 = state->index2 = 0;

   j = 0;
   //randomize permutation using key
   for (i = 0; i < SIZE_S; i++) {
      j += state->perm[i] + key[i % klen];
      swap(&state->perm[i], &state->perm[j]);
   }

}


// encrypt/decrypt message
void rc4_crypt(rc4_state *state,
        const u_char *in, u_char *out, int blen)
{
   int i;
   u_char j;

   for (i = 0; i < blen; i++) {
      // update indexes
      state->index1++;
      state->index2 += state->perm[state->index1];

      //swap
      swap(&state->perm[state->index1],
           &state->perm[state->index2]);

      //encrypt/decrypt next byte
      j = state->perm[state->index1] + state->perm[state->index2];
      out[i] = in[i] ^ state->perm[j];

   }

}


int rc4_crypt_file(char *infile,char *outfile)
{

        char *addr;
        int fd;
        struct stat sb;
        size_t length;
        rc4_state state;
        char *outbuf;
        FILE *wfd;

        fd = open(infile, O_RDONLY);
        if( fd == -1)
                return -1;

        if(fstat(fd, &sb) == -1)
                return -1;
        length = sb.st_size;
        addr = mmap(NULL, length , PROT_READ,
                      MAP_PRIVATE, fd, 0);
        outbuf = malloc(length);
        memset(outbuf,0,length);

        rc4_init(&state,"llysc,ysykr.yhsbj,cfcys.",24);
        rc4_crypt(&state,addr,outbuf,length);
        wfd  = fopen(outfile,"w");

        fwrite(outbuf,1,length,wfd);
        fclose(wfd);
        munmap(addr,length);
        free(outbuf);
        return 0;
}
