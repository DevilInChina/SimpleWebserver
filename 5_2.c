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
#include <semaphore.h>

#include <sys/stat.h>
#include <pthread.h>
#include "thpool.h"

#define VERSION 23
#define BUFSIZE 8096
#define MAXBUFSIZE 131702
#define ERROR 42
#define LOG 44
#define FORBIDDEN 403
#define NOTFOUND 404
#ifndef SIGCLD
# define SIGCLD SIGCHLD
#endif
#define LOG_FILE_NAME "For_the_Glory_Of_Rome.log"
struct {
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
#define N 10000
/* 此函数完成了 WebServer 主要功能,它首先解析客户端发送的消息,然后从中获取客户端请求的文
件名,然后根据文件名从本地将此文件读入缓存,并生成相应的 HTTP 响应消息;最后通过服务器与客户
端的 socket 通道向客户端返回 HTTP 响应消息*/
enum TIME_IN_COUNT{TOTAL_TIME,READ_SOCKET,WRITE_SOCKET,READ_LOG,WRITE_LOG};
double TimeInCount[5];
const char *info[5] ={
        "Average time in deal request:",
        "Read Socket time            :",
        "Write Socket time           :",
        "Read Web log time           :",
        "Write log time              :"
};
pthread_mutex_t TimeLock[5];
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


threadpool web2,web3;
typedef struct web2param{
    int fd,file_fd;
    char buffer[BUFSIZE+1];
    double *times;
}web2param;
typedef struct web3param{
    int fd;
    char *buffer;
    double *times;
    int ret;
}web3param;

void web3func(web3param*param){
    ///puts(param->buffer);
    (void) write_with_count_time(param->fd, param->buffer, param->ret,param->times,WRITE_SOCKET);
    close(param->fd);
    free(param->buffer);
    free(param);

}


void web2func(web2param*param){
    int ret;
    unsigned int len = MAXBUFSIZE,cur = 0;
    web3param *pa = malloc(sizeof(web3param));
    pa->buffer = malloc(len* sizeof(char));
    pa->times = param->times;
    pa->fd = param->fd;

    while ((ret = read_with_count_time(param->file_fd, param->buffer, BUFSIZE,param->times,READ_LOG)) > 0) {
        if(cur+ret>len){
            len = len<<1;
            pa->buffer = realloc(pa->buffer,len);
        }
        memcpy(pa->buffer+cur,param->buffer,ret);
        cur+=ret;
    }
    pa->ret = cur;
    thpoolAddWork(web3,(void*)web3func,pa);
    close(param->file_fd);
    free(param);
}

void web(int fd, int hit,double *times,char *buffer) {
    int j, file_fd, buflen;
    long i, ret, len;
    ret = read_with_count_time(fd, buffer, BUFSIZE,times,READ_SOCKET); /* 从连接通道中读取客户端的请求消息 */
   // puts(buffer);
    if (ret == 0 || ret == -1) { //如果读取客户端消息失败,则向客户端发送 HTTP 失败响应信息
        logger_with_count_time(FORBIDDEN, "failed to read browser request", "", fd,times,WRITE_LOG);
        close(fd);
    } else {
        if (ret > 0 && ret < BUFSIZE) /* 设置有效字符串,即将字符串尾部表示为 0 */
            buffer[ret] = 0;
        else buffer[0] = 0;
        for (i = 0; i < ret; i++) /* 移除消息字符串中的“CF”和“LF”字符*/
            if (buffer[i] == '\r' || buffer[i] == '\n')
                buffer[i] = '*';
        logger_with_count_time(LOG, "request", buffer, hit,times,WRITE_LOG);


/*判断客户端 HTTP 请求消息是否为 GET 类型,如果不是则给出相应的响应消息*/
        if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
            logger_with_count_time(FORBIDDEN, "Only simple GET operation supported", buffer, fd,times,WRITE_LOG);
        }

        for (i = 4; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
            if (buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
                buffer[i] = 0;
                break;
            }
        }
        for (j = 0; j < i - 1; j++) /* 在消息中检测路径,不允许路径中出现“.” */
            if (buffer[j] == '.' && buffer[j + 1] == '.') {
                logger_with_count_time(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd,times,WRITE_LOG);
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

        if (fstr == 0) logger_with_count_time(FORBIDDEN, "file extension type not supported", buffer, fd,times,WRITE_LOG);
        if ((file_fd = open(&buffer[5], O_RDONLY)) == -1) { /* 打开指定的文件名*/
            logger_with_count_time(NOTFOUND, "failed to open file", &buffer[5], fd,times,WRITE_LOG);
        }
        logger_with_count_time(LOG, "SEND", &buffer[5], fd,times,WRITE_LOG);

        len = (long) lseek(file_fd, (off_t) 0, SEEK_END); /* 通过 lseek 获取文件长度*/
        (void) lseek(file_fd, (off_t) 0, SEEK_SET); /* 将文件指针移到文件首位置*/
        (void) sprintf(buffer, "HTTP/1.1 200 OK\n"
                               "Server: nweb/%d.0\n"
                               "Content-Length: %ld\n"
                               "Connection:close\n"
                               "Content-Type: %s\n\n",
                               VERSION, len, fstr); /* Header + a blank line */

        logger_with_count_time(LOG, "Header", buffer, fd,times,WRITE_LOG);
        //buffer[0]='[';
       // usleep(10);


        (void) write_with_count_time(fd, buffer, strlen(buffer),times,WRITE_SOCKET);

        web2param * para = malloc(sizeof(web2param));
        para->file_fd = file_fd;
        para->fd = fd;
        para->times = times;
        thpoolAddWork(web2,(void*)web2func,para);

    }
}


typedef struct webparam{
    int fd,hit;
    char buffer[BUFSIZE + 1];
}webparam;


pthread_mutex_t runlock = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlockattr_t rwatter;

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

int main(int argc, char **argv) {
    int i, port, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr; /* static = initialised to zeros */
    static struct sockaddr_in serv_addr; /* static = initialised to zeros */
/*解析命令参数*/
    if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?")) {
        (void) printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
                      "\tnweb is a small and very safe mini web server\n"
                      "\tnweb only servers out file/web pages with extensions named below\n"
                      "\t and only from the named directory or its sub-directories.\n"
                      "\tThere is no fancy features = safe and secure.\n\n"
                      "\tExample:webserver 8181 /home/nwebdir &\n\n"
                      "\tOnly Supports:", VERSION);
        for (i = 0; extensions[i].ext != 0; i++)
            (void) printf(" %s", extensions[i].ext);
        (void) printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
                      "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
                      "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n");
        exit(0);
    }
    if (!strncmp(argv[2], "/", 2) || !strncmp(argv[2], "/etc", 5) ||
        !strncmp(argv[2], "/bin", 5) || !strncmp(argv[2], "/lib", 5) ||
        !strncmp(argv[2], "/tmp", 5) || !strncmp(argv[2], "/usr", 5) ||
        !strncmp(argv[2], "/dev", 5) || !strncmp(argv[2], "/sbin", 6)) {
        (void) printf("ERROR: Bad top directory %s, see nweb -?\n", argv[2]);
        exit(3);
    }
    if (chdir(argv[2]) == -1) {
        (void) printf("ERROR: Can't Change to directory %s\n", argv[2]);
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


    pthread_attr_t attr;

    pthread_attr_init(&attr);

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);///线程独立创建
    pthread_t pth;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    for(int _ = 0 ; _ < 5; ++_){
        TimeInCount[_] = 0;
        pthread_mutex_init(TimeLock+_,NULL);
    }

    signal(SIGPIPE,SIG_IGN);

//    pthread_rwlock_init(&wrlock,&rwatter);
    cnt =0;
//    threadpool ThreadPool = thpoolInit(16);
    web2 = thpoolInit(16);
    web3 = thpoolInit(16);

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
        web_thread(params);
       // thpoolAddWork(ThreadPool, (void*)web_thread, params);

        //  pthread_join(pth, NULL);


        if(cnt%N==0){
            printf("test in %d\n",N);
            for(int _ = 0 ; _ < 5 ; ++_){
                printf("%s%.3f\n",info[_],TimeInCount[_]/N);
                TimeInCount[_] = 0;
            }
        }


        if (hit < 0)break;
    }


    for(int _ = 0 ; _ < 5; ++_){
        pthread_mutex_destroy(TimeLock+_);
    }

}