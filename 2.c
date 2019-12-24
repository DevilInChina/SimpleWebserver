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


#define VERSION 23
#define BUFSIZE 8096
#define ERROR 42
#define LOG 44
#define FORBIDDEN 403
#define NOTFOUND 404
#ifndef SIGCLD
# define SIGCLD SIGCHLD
#endif
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
    if ((fd = open("webserver.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0) {
        (void) write(fd, logbuffer, strlen(logbuffer));
        (void) write(fd, "\n", 1);
        (void) close(fd);
    }
  //  free(stime);
}

#define N 1000

enum TIME_IN_COUNT{TOTAL_TIME,READ_SOCKET,WRITE_SOCKET,READ_LOG,WRITE_LOG};
double *TimeInCount;

sem_t *psem;
const char *info[5] ={
        "Average time in deal request:",
        "Read Socket time            :",
        "Write Socket time           :",
        "Read Web log time           :",
        "Write log time              :"
};

int read_with_count_time(int fd,char *buffer,int BufferSize,double *Time,enum TIME_IN_COUNT POS){
    struct timeval t1,t2;
    gettimeofday(&t1,NULL);
    int ret = read(fd,buffer,BufferSize);
    gettimeofday(&t2,NULL);
    sem_wait(psem);
    *(Time+POS)+=getTimePast(&t1,&t2);
    sem_post(psem);
    return ret;
}

void logger_with_count_time(int type,char *s1,char*s2,int socket_fd,double *Time,enum TIME_IN_COUNT POS){
    struct timeval t1,t2;
    gettimeofday(&t1,NULL);
    logger(type,s1,s2,socket_fd);
    gettimeofday(&t2,NULL);
    sem_wait(psem);
    *(Time+POS)+=getTimePast(&t1,&t2);
    sem_post(psem);
}

int write_with_count_time(int fd,char *buffer,int BufferSize,double *Time,enum TIME_IN_COUNT POS){
    struct timeval t1,t2;
    gettimeofday(&t1,NULL);
    int ret = write(fd,buffer,BufferSize);
    gettimeofday(&t2,NULL);
    sem_wait(psem);
    *(Time+POS)+=getTimePast(&t1,&t2);
    sem_post(psem);
    return ret;
}


void web(int fd, int hit,double *times) {
    int j, file_fd, buflen;
    long i, ret, len;
    char *fstr;
    char buffer[BUFSIZE + 1]; /* 设置静态缓冲区 */
    ret = read_with_count_time(fd, buffer, BUFSIZE,times,READ_SOCKET); /* 从连接通道中读取客户端的请求消息 */
    // puts(buffer);
    if (ret == 0 || ret == -1) { //如果读取客户端消息失败,则向客户端发送 HTTP 失败响应信息
        logger_with_count_time(FORBIDDEN, "failed to read browser request", "", fd,times,WRITE_LOG);
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
        fstr = (char *) 0;
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

        /* 不停地从文件里读取文件内容,并通过 socket 通道向客户端返回文件内容*/
        while ((ret = read_with_count_time(file_fd, buffer, BUFSIZE,times,READ_LOG)) > 0) {
            (void) write_with_count_time(fd, buffer, ret,times,WRITE_SOCKET);
        }


        //  sleep(1); /* sleep 的作用是防止消息未发出,已经将此 socket 通道关闭*/
        close(file_fd);
    }
    close(fd);
}
typedef struct webparam{
    int fd,hit;
}webparam;

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
    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        logger(ERROR, "system call", "bind", 0);
    if (listen(listenfd, 64) < 0)
        logger(ERROR, "system call", "listen", 0);
    int cnt = 0;
    int last = 0;

    if((psem=sem_open("2017011272",O_CREAT,0666,1)) == SEM_FAILED){
        perror("create semaphore error");
        exit(1);
    }

    int shm_fd;
    if((shm_fd = shm_open("mmap_2017011272",O_RDWR|O_CREAT,0666))<0){
        perror("create shared memory obj error");
        exit(2);
    }

    ftruncate(shm_fd, sizeof(double)*(N+1));

    TimeInCount = (double*)mmap(NULL, sizeof(double)*(5),PROT_READ|PROT_WRITE,MAP_SHARED,shm_fd,0);
    for(int _ = 0 ; _ < 5 ; ++_)TimeInCount[_] = 0;
    if(TimeInCount==MAP_FAILED){
        perror("create mmap error");
        exit(0);
    }

    for (hit = 1;; hit++) {

        length = sizeof(cli_addr);
        if ((socketfd = accept(listenfd, (struct sockaddr *) &cli_addr, &length)) < 0) {
            logger(ERROR, "system call", "accept", 0);
            continue;
        }

        ++cnt;
        pid_t This_pid = fork();
        if(This_pid==0) {
            ///printf("deal %d\n",getpid());
            ///son
            struct timeval t1,t2;

            gettimeofday(&t1,NULL);
            web(socketfd, hit,TimeInCount); /* never returns */
            gettimeofday(&t2,NULL);

            sem_wait(psem);
            *(TimeInCount+TOTAL_TIME)+=getTimePast(&t1,&t2);
            sem_post(psem);
            exit(0);

        }else{
            if(cnt%N==0){
                int s = 0;
                for(int k = last ; k < cnt ; ++k){
                    int ks = wait(NULL);
                    s+=ks;
                }
                //sleep(1);
                printf("In test %d %d\n",N,s);
                for(int _ = 0; _ < 5 ; ++_){
                    printf("%s%.3f\n",info[_],TimeInCount[_]/N);
                    TimeInCount[_] = 0;
                }
                last = cnt;
            }
        }
    }
}
