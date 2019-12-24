/*Server Code*/
/* webserver.c*/
/*The following main code from https://github.com/ankushagarwal/nweb*, but they are modified
slightly*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <sys/wait.h>

#include <sys/stat.h>
#include <pthread.h>
#include "thpool.h"
#include "jwHash.h"
#include "shift.h"
#include "shift.h"
#include "filesys.h"

#define THREAD_NUMBERS_FIRST 1
#define THREAD_NUMBERS_SECOND 32
#define THREAD_NUMBERS_THIRD 1

#define VERSION 23
#define BUFSIZE 131702
#define MAXPAGESIZE HEAPSIZE
#define ERROR 42
#define LOG 44
#define FORBIDDEN 403
#define NOTFOUND 404

#define N 10000
#define HASHSIZE 100007
#ifndef SIGCLD
# define SIGCLD SIGCHLD
#endif
#define LOG_FILE_NAME "For_the_Glory_Of_Rome.log"
struct{
    char *ext;
    char *filetype;
} extensions [] = {
        {"gif", "image/gif" },
        {"jpg", "image/jpg" },
        {"jpeg","image/jpeg"},
        {"png", "image/png" },
        {"ico", "image/ico" },
        {"zip", "image/zip" },{"gz", "image/gz" },
        {"tar", "image/tar" },
        {"htm", "text/html" },
        {"html","text/html" },
        {0,0} };
enum ShiftFunc{LRU,MQ,LFU,ARC};
#if (!defined(__LFU__) && !defined(__MQ__) && !defined(__LRU__) && !defined(__ARC__))
#define __ARC__
#endif

#if (defined(__LRU__))
#define CHOOSE LRU
LRUQueue
#elif (defined(__MQ__))
#define CHOOSE MQ
MQueue
#elif (defined(__LFU__))
#define CHOOSE LFU
LFUHeap
#elif (defined(__ARC__))
#define CHOOSE ARC
ARCQH
#endif
        Que;
File_Sys FS;
void (*InitFunc[6])(ShiftParameter*);
void maininit(ShiftParameter *para){
    InitFunc[0] = LRUQueue_Init;
    InitFunc[1] = MQueue_Init;
    InitFunc[2] = LFUHeap_Init;
    InitFunc[3] = ARCQH_Init;

    InitFunc[CHOOSE](para);

}/// a store data struct


/* 日志函数,将运行过程中的提示信息记录到 webserver.log 文件中*/
double getTimePast(struct timeval *t1,struct timeval*t2){
    return (t2->tv_sec - t1->tv_sec) * 1000.0 + (t2->tv_usec - t1->tv_usec) / 1000.0;
}
void logger(int type, char *s1, char *s2, int socket_fd) {
    int fd;
    char logbuffer[BUFSIZE * 2];
    time_t t1;
    time(&t1);
    char stime[20];
    strcpy(stime,ctime(&t1));
    //malloc(sizeof(int));

    /*根据消息类型,将消息放入 logbuffer 缓存,或直接将消息通过 socket 通道返回给客户端*/
    switch (type) {
        case ERROR:
            (void) sprintf(logbuffer, "%s:ERROR: %s:%s Errno=%d exiting pid=%d",stime, s1, s2, errno, getpid());
            break;
        case FORBIDDEN:
            (void) write(socket_fd,
                         "Forbidden to visit.\n",
                         271);
            (void) sprintf(logbuffer, "%s:FORBIDDEN: %s:%s",stime, s1, s2);
            break;
        case NOTFOUND:
            (void) write(socket_fd,
                         "Not Found.\n",
                         224);
            (void) sprintf(logbuffer, "%s:NOT FOUND: %s:%s",stime,s1, s2);
            break;
        case LOG:
            (void) sprintf(logbuffer, "%s:INFO: %s:%s:%d",stime, s1, s2, socket_fd);
            break;
    }
/* 将 logbuffer 缓存中的消息存入 webserver.log 文件*/
    if ((fd = open(LOG_FILE_NAME, O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0) {
        (void) write(fd, logbuffer, strlen(logbuffer));
        (void) write(fd, "\n", 1);
        (void) close(fd);
    }
  //  free(stime);
}
/* 此函数完成了 WebServer 主要功能,它首先解析客户端发送的消息,然后从中获取客户端请求的文
件名,然后根据文件名从本地将此文件读入缓存,并生成相应的 HTTP 响应消息;最后通过服务器与客户
端的 socket 通道向客户端返回 HTTP 响应消息*/
#define N_CLK 6
enum TIME_IN_COUNT{TOTAL_TIME,READ_SOCKET,WRITE_SOCKET,READ_LOG,WRITE_LOG,SAVE_BUF};
double TimeInCount[N_CLK];
const char *info[N_CLK] ={
        "Average time in deal request:",
        "Read Socket time            :",
        "Write Socket time           :",
        "Read Web log time           :",
        "Write log time              :",
        "Save buffer time            :",
};
pthread_mutex_t TimeLock[N_CLK];
int read_with_count_time(int fd,char *buffer,int BufferSize,double *Time,enum TIME_IN_COUNT POS){
    struct timeval t1,t2;
    gettimeofday(&t1,NULL);
    int ret = read(fd,buffer,BufferSize);
    gettimeofday(&t2,NULL);
    pthread_mutex_lock(TimeLock+POS);
    *(Time+POS)+=getTimePast(&t1,&t2);
    pthread_mutex_unlock(TimeLock+POS);
    return ret;
}

void logger_with_count_time(int type,char *s1,char*s2,int socket_fd,double *Time,enum TIME_IN_COUNT POS){
    struct timeval t1,t2;
    gettimeofday(&t1,NULL);
    logger(type,s1,s2,socket_fd);
    gettimeofday(&t2,NULL);
    pthread_mutex_lock(TimeLock+POS);
    *(Time+POS)+=getTimePast(&t1,&t2);
    pthread_mutex_unlock(TimeLock+POS);
}

int read_web_log_with_count_time(int name,char *buffer,int BufferSize,double *Time,enum TIME_IN_COUNT POS){
    struct timeval t1,t2;
    gettimeofday(&t1,NULL);
    int ret = FS.read(&FS,buffer,name);
    //int ret = read(fd,buffer,BufferSize);
    gettimeofday(&t2,NULL);
    pthread_mutex_lock(TimeLock+POS);
    *(Time+POS)+=getTimePast(&t1,&t2);
    pthread_mutex_unlock(TimeLock+POS);
    return ret;
}

int write_with_count_time(int fd,const char *buffer,int BufferSize,double *Time,enum TIME_IN_COUNT POS){
    struct timeval t1,t2;
    gettimeofday(&t1,NULL);

    int ret = write(fd,buffer,BufferSize);

    gettimeofday(&t2,NULL);
    pthread_mutex_lock(TimeLock+POS);
    *(Time+POS)+=getTimePast(&t1,&t2);
    pthread_mutex_unlock(TimeLock+POS);
    return ret;
}

typedef struct hashTableNode{

    char *buffer;
    int Done_Of_Read;
    Node *loc;

}hashTableNode;

jwHashTable *hashTable;

threadpool web2,web3;

typedef struct web2param{
    int fd,file_fd;
    StoreNode *key;
    hashTableNode*info;
    double *times;
    int len;
}web2param;



void web3func(web2param*param){
    close(param->file_fd);
    while (!param->info->Done_Of_Read);
    (void) write_with_count_time(param->fd, param->info->buffer, param->len,param->times,WRITE_SOCKET);
    close(param->fd);
    free(param);
}
int PageFound,PageShift;

int tot;
void hashNodeDestry(void *p){
   // puts(((hashTableNode*)p)->buffer);
    --tot;
    free(((hashTableNode*)p)->buffer);
    free(((hashTableNode*)p));
}


void web2func(web2param*param) {
    param->len=read_web_log_with_count_time(param->file_fd,param->info->buffer,param->len,param->times,READ_LOG);
    param->info->Done_Of_Read = 1;
    thpool_add_work(web3, (void *) web3func, param);
    //web3func(param);
}

int findAndLoadPage(web2param*para){
    hashTableNode*WebInfo;
    struct timeval t1,t2;
    gettimeofday(&t1,NULL);
    StoreNode*victim;

    int ret = get_ptr_by_str(hashTable,para->key->name,(void*)&WebInfo);

    if(ret==HASHNOTFOUND){
        para->info = malloc(sizeof(hashTableNode));

        para->info->buffer = malloc((1+para->len)* sizeof(char));

        para->info->Done_Of_Read = 0;
        //web2func(para);
        thpool_add_work(web2,(void*)web2func,para);

        sem_wait(&Que.outLock);

        Que.AddFirst(&Que,para->key);

        //printf("push %s\n",para->key->name);
        para->info->loc = Que.ls.last_added(&Que.ls);

        ++PageShift;

        add_ptr_by_str(hashTable,para->key->name,para->info);

        while (Que.len>MAXPAGESIZE){
            victim = Que.ls.victim(&Que.ls);
          //  printf("pop %s\n",para->key->name);

            del_by_str(hashTable,(victim)->name,hashNodeDestry);

            Que.PopLast(&Que);
        }
        sem_post(&Que.outLock);

    }else{

        //printf("adjust %s\n",para->key->name);
        free(para->key);

        para->info = WebInfo;
        sem_wait(&Que.outLock);

        Que.ChangePriority(&Que,WebInfo->loc);

        ++PageFound;
        sem_post(&Que.outLock);
        thpool_add_work(web3,(void*)web3func,para);
        //web3func(para);
    }
    gettimeofday(&t2,NULL);
    pthread_mutex_lock(TimeLock+SAVE_BUF);
    *(TimeInCount+SAVE_BUF)+=getTimePast(&t1,&t2);
    pthread_mutex_unlock(TimeLock+SAVE_BUF);

    return ret;
}

web2param* webPredeal(int fd,int hit,double *times,char *buffer) {
    int j, buflen;
    long i, ret, len;
    web2param *para = malloc(sizeof(web2param));
    para->file_fd = -1;
    para->fd = fd;
    para->times = times;
    para->key = NULL;
    ret = read_with_count_time(fd, buffer, BUFSIZE, times, READ_SOCKET); /* 从连接通道中读取客户端的请求消息 */
    // puts(buffer);
    if (ret == 0 || ret == -1) { //如果读取客户端消息失败,则向客户端发送 HTTP 失败响应信息
        logger_with_count_time(FORBIDDEN, "failed to read browser request", "", fd, times, WRITE_LOG);
        close(fd);
    } else {

        if (ret > 0 && ret < BUFSIZE) /* 设置有效字符串,即将字符串尾部表示为 0 */
            buffer[ret] = 0;
        else buffer[0] = 0;

        for (i = 0; i < ret; i++) /* 移除消息字符串中的“CF”和“LF”字符*/
            if (buffer[i] == '\r' || buffer[i] == '\n')
                buffer[i] = '*';
        logger_with_count_time(LOG, "request", buffer, hit, times, WRITE_LOG);

/*判断客户端 HTTP 请求消息是否为 GET 类型,如果不是则给出相应的响应消息*/
        if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
            logger_with_count_time(FORBIDDEN, "Only simple GET operation supported", buffer, fd, times, WRITE_LOG);
        }

        for (i = 4; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
            if (buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
                buffer[i] = 0;
                break;
            }
        }
        for (j = 0; j < i - 1; j++) /* 在消息中检测路径,不允许路径中出现“.” */
            if (buffer[j] == '.' && buffer[j + 1] == '.') {
                logger_with_count_time(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd, times,
                                       WRITE_LOG);
            }
        if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6))
/* 如果请求消息中没有包含有效的文件名,则使用默认的文件名 index.html */
            (void) strcpy(buffer, "GET /index.html");
/* 根据预定义在 extensions 中的文件类型,检查请求的文件类型是否本服务器支持 */
        buflen = strlen(buffer);
        char *fstr = (char *) 0;
        for (i = 0; extensions[i].ext != 0; i++) {
            len = strlen(extensions[i].ext);
            if (!strncmp(&buffer[buflen - len], extensions[i].ext, len)) {
                fstr = extensions[i].filetype;
                break;
            }
        }


        if (fstr == 0)
            logger_with_count_time(FORBIDDEN, "file extension type not supported", buffer, fd, times, WRITE_LOG);
        //printf("bef opr\n");
        if ((len = FS.getLen(&FS,buffer+5)) == -1) { /* 打开指定的文件名*/
            logger_with_count_time(NOTFOUND, "failed to open file", &buffer[5], fd, times, WRITE_LOG);
        } else {
            para->key = malloc(sizeof(StoreNode));
            strcpy(para->key->name, buffer + 5);
            para->file_fd = FS.toInt(buffer+5,1);
            logger_with_count_time(LOG, "SEND", &buffer[5], fd, times, WRITE_LOG);


            (void) sprintf(buffer, "HTTP/1.1 200 OK\n"
                                   "Server: nweb/%d.0\n"
                                   "Content-Length: %ld\n"
                                   "Connection:close\n"
                                   "Content-Type: %s\n\n",
                           VERSION, len, fstr); /* Header + a blank line */

            logger_with_count_time(LOG, "Header", buffer, fd, times, WRITE_LOG);



            (void) write_with_count_time(fd, buffer, strlen(buffer), times, WRITE_SOCKET);

            para->len = len;
        }
    }
    return para;
}


void web(int fd, int hit,double *times,char *buffer) {
    web2param*para = webPredeal(fd,hit,times,buffer);
    if (para->file_fd != -1) {
        findAndLoadPage(para);

        //web2func(para);
    } else {
        free(para);
        free(para->key);
        close(fd);
    }

}


typedef struct webparam{
    int fd,hit;
    char buffer[BUFSIZE + 1];
}webparam;



void *web_thread(webparam*data) {

    struct timeval t1,t2;
    gettimeofday(&t1,NULL);

    web(data->fd, data->hit,TimeInCount,data->buffer);
    free(data);
    gettimeofday(&t2,NULL);

    pthread_mutex_lock(TimeLock+TOTAL_TIME);
    *(TimeInCount+TOTAL_TIME)+=getTimePast(&t1,&t2);
    pthread_mutex_unlock(TimeLock+TOTAL_TIME);

    return 0;
}


#define MAXTESsT 10000000
#ifdef MAXTEST
pthread_mutex_t loc = PTHREAD_MUTEX_INITIALIZER;
volatile int tot2 =0 ,tot3 =0 ;
void func(void *s){
    web2param *ss=s;

    free(ss->key);
    free(ss);
};
int num[1104000];

void toStr(char *s,int a) {
    int cnt = 0;
    ++a;
    while (a) {
        s[cnt] = a % 10 + '0';
        ++cnt;
        a /= 10;
    }
    s[cnt] = 0;
}

int DEBUG_findAndLoadPage(web2param*para) {
    hashTableNode *WebInfo;
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    StoreNode *victim;

    int ret = get_ptr_by_str(hashTable, para->key->name, (void *) &WebInfo);

    if (ret == HASHNOTFOUND) {
        para->info = malloc(sizeof(hashTableNode));

        para->info->buffer = malloc(para->len * sizeof(char));

        para->info->Done_Of_Read = 0;
        //web2func(para);
        //thpool_add_work(web2,(void*)web2func,para);

        sem_wait(&Que.outLock);

        Que.AddFirst(&Que, para->key);

        //printf("push %s\n",para->key->name);
        para->info->loc = Que.ls.last_added(&Que.ls);

        ++PageShift;

        add_ptr_by_str(hashTable, para->key->name, para->info);

        while (Que.len > MAXPAGESIZE) {
            victim = Que.ls.victim(&Que.ls);
            //      printf("pop %s\n",para->key->name);

            del_by_str(hashTable, (victim)->name, hashNodeDestry);

            Que.PopLast(&Que);
        }
        sem_post(&Que.outLock);

    } else {

        //    printf("adjust %s\n",para->key->name);
        free(para->key);

        para->info = WebInfo;
        sem_wait(&Que.outLock);

        Que.ChangePriority(&Que, WebInfo->loc);

        ++PageFound;
        sem_post(&Que.outLock);
        //thpool_add_work(web3,(void*)web3func,para);
        //web3func(para);
    }
    gettimeofday(&t2, NULL);
    pthread_mutex_lock(TimeLock + SAVE_BUF);
    *(TimeInCount + SAVE_BUF) += getTimePast(&t1, &t2);
    pthread_mutex_unlock(TimeLock + SAVE_BUF);

    return ret;
}

#endif
int Cmp_In_Lfu(const void *a,const void *b){
    const StoreNode *A= a;
    const StoreNode *B = b;
    if(A->Key==B->Key)return 0;
    else if(A->Key<B->Key){
        return 1;
    }else {
        return -1;
    }
}

double LFUChange(double a){
    return 0.5*a+1;
}

int main(int argc, char **argv) {
    ShiftParameter s;
    s.change = LFUChange;
    s.Store = &Que;
    s.Cmp = Cmp_In_Lfu;
    maininit(&s);
    File_Sys_Init(&FS,argv[2],argv[3],THREAD_NUMBERS_SECOND);

    hashTable = create_hash(HASHSIZE);
    pthread_attr_t attr;

    pthread_attr_init(&attr);

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);///线程独立创建

    threadpool ThreadPool = thpool_init(THREAD_NUMBERS_FIRST);
    web2 = thpool_init(THREAD_NUMBERS_SECOND);
    web3 = thpool_init(THREAD_NUMBERS_THIRD);
#ifdef MAXTEST

    int scnt = 0;
    for(int i =0 ; i < 100000 ; ++i){
        if(i<4000){
            for(int j = 0 ; j < 128 ; ++j){
                num[scnt++] = i;
            }
        }else if(i < 20000){
            for(int j = 0 ; j < 32 ; ++j){
                num[scnt++] = i;
            }
        }else{
            num[scnt++] = i;
        }
    }
    srand(time(NULL));
    {
        threadpool temp = thpool_init(8);
        for(int i = 0 ; i < MAXTEST ; ++i){
            web2param*param = malloc(sizeof(web2param));
            
            param->key = malloc(sizeof(StoreNode));

            toStr(param->key->name,num[rand()%scnt]);
           // param->key->name[31] = 0;
            param->len = 180000;
            ++tot2;
            ++tot3;
            thpool_add_work(temp,(void*)DEBUG_findAndLoadPage,param);
            //DEBUG_findAndLoadPage(param);
            //DEBUG_findAndLoadPage(param);
            //printf("%p\n",param);
            //thpool_add_work(temp,(void*)func,param);

        }
        int tots = 0;
        int siz = 0;
        for(int i = 0 ; i < HASHSIZE;++i){
            if(hashTable->bucket[i]){
                jwHashEntry * k = hashTable->bucket[i];
                while (k){
                    hashTableNode *p= k->value.ptrValue;
                    tots+=strlen(p->buffer);
                    k = k->next;
                    ++siz;
                }
            }
        }
        thpool_destroy(temp);
        printf("%d %d %d %d %d %d %d %f\n",PageShift,PageFound,tot,tot2,tot3,1,siz,tots/1024.0/1024);
    }

    sleep(1000);
#endif
    int i, port, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr; /* static = initialised to zeros */
    static struct sockaddr_in serv_addr; /* static = initialised to zeros */
/*解析命令参数*/

    if (chdir(argv[4]) == -1) {
        (void) printf("ERROR: Can't Change to directory %s\n", argv[4]);
        exit(4);
    }

/* 建立服务端侦听 socket*/
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        logger(ERROR, "system call", "socket", 0);
    port = atoi(argv[1]);
    if (port < 0 || port > 60000)
        logger(ERROR, "Invalid port number (try 1->60000)", argv[1], 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        logger(ERROR, "system call", "bind", 0);
        puts("err 3");
        return 3;
    }

    if (listen(listenfd, 64) < 0) {
        logger(ERROR, "system call", "listen", 0);
        puts("err 4");
        return 4;
    }
    int cnt = 0;
    int last = 0;


    pthread_t pth;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    PageFound = PageShift = 0;
    for(int _ = 0 ; _ < N_CLK; ++_){
        TimeInCount[_] = 0;
        pthread_mutex_init(TimeLock+_,NULL);
    }

    signal(SIGPIPE,SIG_IGN);

//    pthread_rwlock_init(&wrlock,&rwatter);
    cnt =0;

    for (hit = 1;; ++hit) {
        length = sizeof(cli_addr);
        if ((socketfd = accept(listenfd, (struct sockaddr *) &cli_addr, &length)) < 0) {
            logger(ERROR, "system call", "accept", 0);
            continue;
        }
        ++cnt;
        webparam *params = (webparam *) malloc(sizeof(webparam));
        params->hit = hit;
        params->fd = socketfd;
       // web_thread(params);

        thpool_add_work(ThreadPool, (void*)web_thread, params);
        //web_thread(params);
        //  pthread_join(pth, NULL);
        if(cnt%N==0){
            printf("test in %d ,page size %d ,shift rate %.2f%%",N,MAXPAGESIZE,100.0*PageShift/(PageShift+PageFound));
#if(defined(__LRU__))
            printf(",LRU\n");
#elif (defined(__LFU__))
            printf(",LFU\n");
#elif (defined(__MQ__))
            printf(",LRU queue number %d",MQUEUE);
#elif (defined(__ARC__))
            printf(",Max times stay in lru:%d\n",LRUMAXTIMES);
            printf("LRU Queue length:%d\n",Que.listLen);
#endif
            //TimeInCount[SAVE_BUF]-=TimeInCount[READ_LOG];
            for(int _ = 0 ; _ < N_CLK ; ++_){
                printf("%s%.3f\n",info[_],TimeInCount[_]/N);
                TimeInCount[_] = 0;
            }
            PageShift = PageFound = 0;
        }


        if (hit < 0)break;
    }



    for(int _ = 0 ; _ < 5; ++_){
        pthread_mutex_destroy(TimeLock+_);
    }

}