//
// Created by devilinchina on 12/23/19.
//

#ifndef WEBSERVER_FILESYS_H
#define WEBSERVER_FILESYS_H

#include "list.h"

typedef struct File_Sys File_Sys;
struct FileNode{
    int fd;
    volatile int inUse;
};
struct File_Sys{
    sem_t lock;
    List ls;
    int tot;
    char filePath[256];
    int fdcnt;
    long long sum[101517];
    int (*read)(File_Sys*fs,char *buf,int name);
    int (*toInt)(const char *,int);
    int (*getLen)(File_Sys*fs,const char *name);
};

void File_Sys_Init(File_Sys *fs,const char *filePath,const char *handlePath,int size);
#endif //WEBSERVER_FILESYS_H
