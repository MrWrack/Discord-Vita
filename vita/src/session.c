#include <psp2/io/fcntl.h>
#include <string.h>
#include "session.h"
#define SESSION_DIR "ux0:data/DiscordVita"
#define SESSION_FILE "ux0:data/DiscordVita/session.dat"

int dv_session_load(char *out,unsigned int size){
    if(!out || size<2) return -1;
    SceUID fd=sceIoOpen(SESSION_FILE,SCE_O_RDONLY,0);
    if(fd<0){out[0]=0; return -2;}
    int n=sceIoRead(fd,out,size-1);
    sceIoClose(fd);
    if(n<=0){out[0]=0; return -3;}
    out[n]=0; return 0;
}

int dv_session_save(const char *s){
    if(!s || !s[0]) return -1;
    sceIoMkdir(SESSION_DIR,0777);
    SceUID fd=sceIoOpen(SESSION_FILE,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0600);
    if(fd<0) return -2;
    int len=(int)strlen(s);
    int n=sceIoWrite(fd,s,len);
    sceIoClose(fd);
    return n==len?0:-3;
}

void dv_session_clear(void){ sceIoRemove(SESSION_FILE); }
