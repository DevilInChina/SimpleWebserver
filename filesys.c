#include "filesys.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/user.h>

int File_SysToInt(const char *s,int cond){
    int ret = 0;
    const char *b = s;
    while (*b){
        if(!isdigit(*b)){
            if(cond)break;
            else return -1;
        }
        ret = ret*10+*b-'0';
        ++b;
    }
    return ret;
}
// /media/devilinchina/Backup/NewFileSys/data.txt /media/devilinchina/Backup/NewFileSys/handle.txt /media/devilinchina/Backup/NewFileSys/
int File_SysRead(File_Sys*fs,char *buf,int name){
    int len = fs->sum[name] - fs->sum[name-1];
    struct FileNode * added;
    sem_wait(&fs->lock);
    if(fs->ls.begin(&fs->ls)==fs->ls.end(&fs->ls)){
        added = malloc(sizeof(struct FileNode));
        fs->ls.push_front(&fs->ls,added);
        added->fd = open(fs->filePath,O_RDONLY);
        added->inUse = 1;
        ++fs->fdcnt;
    }else{
        added = fs->ls.back(&fs->ls);
        Node *c = fs->ls.cat_last(&fs->ls);
        fs->ls.shift(&fs->ls,c);
        if(added->inUse){
            added = malloc(sizeof(struct FileNode));
            fs->ls.push_front(&fs->ls,added);
            added->fd = open(fs->filePath,O_RDONLY);
            added->inUse = 1;
            ++fs->fdcnt;
        }
    }
    sem_post(&fs->lock);
    int fd = open(fs->filePath,O_RDONLY);
    lseek(fd,fs->sum[name-1],SEEK_SET);
    read(fd,buf,len);
    buf[len] = 0;
    added->inUse = 0;
    return len;
}


int File_Sys_getLen(File_Sys*fs,const char *name){
    char buff[15];
    int c = strlen(name);
    if(c>10)
        return -1;
    strcpy(buff,name);

    buff[c-5] = 0;
    int ret = File_SysToInt(buff,0);
   /// printf("%d\n",ret);
    if(ret>=1 && ret <= 101516){
        return fs->sum[ret]-fs->sum[ret-1];
    }else {
        return -1;
    }
}
void File_Sys_Init(File_Sys *fs,const char *filePath,const char *handlePath,int size){
    sem_init(&fs->lock,getpid(),1);
    //List_init(&fs->ls);
    fs->toInt = File_SysToInt;
    fs->read = File_SysRead;
    fs->getLen=File_Sys_getLen;
    strcpy(fs->filePath,filePath);
    FILE*fp = fopen(handlePath,"r");
    fs->tot = 1;
    fs->sum[0] = 0;
    char buff[16];
    long long sz = 0;
    List_init(&fs->ls);
    fs->fdcnt = 0;
    while (fscanf(fp,"%s %lld",buff,&sz)!=EOF){
        fs->sum[fs->tot++] = sz;
    }
    for(int i = 1 ; i < fs->tot ; ++i){
        fs->sum[i]+=fs->sum[i-1];
    }
    fclose(fp);

}