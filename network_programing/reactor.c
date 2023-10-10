#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/sendfile.h>

#define BUFFER_LENGTH     1024
#define EVENT_LENGTH      512
#define HTTP_WEB_ROOT     "/home/lizhenlin/projects/C++/lingsheng/MyLearning/network_programing"


typedef int (*ZVCALLBACK)(int fd, int event, void *arg);
int recv_cb(int fd, int event, void *arg);   // 前向声明

typedef struct zv_connect_s {
    int fd;
    ZVCALLBACK cb;
    char rbuffer[BUFFER_LENGTH];
    int rc;
    char wbuffer[BUFFER_LENGTH];
    int wc;
    int count;

    char resource[BUFFER_LENGTH];

    int enable_sendfile;
} zv_connect_t;

typedef struct zv_connblock_s {
    zv_connect_t *block;
    struct zv_connblock_s *next;
} zv_connblock_t;

typedef struct zv_reactor_s {
    int epfd;
    int blkcnt;

    zv_connblock_t *blockheader;
} zv_reactor_t;

int zv_init_reactor(zv_reactor_t *reactor)
{
    if (!reactor) return -1;
    
    reactor->blockheader = malloc(sizeof(zv_connblock_t) + EVENT_LENGTH * sizeof(zv_connect_t));
    if (reactor->blockheader == NULL) return -1;

    reactor->blockheader->block = (zv_connect_t*)(reactor->blockheader + 1);
    reactor->blkcnt = 1;
    reactor->blockheader->next = NULL;

    reactor->epfd = epoll_create(1);
}

int zv_connblock_expand(zv_reactor_t *reactor)
{
    if (!reactor) return -1;
    zv_connblock_t *blk = reactor->blockheader;

    // malloc block
    zv_connblock_t *connblock = malloc(sizeof(zv_connblock_t) + EVENT_LENGTH * sizeof(zv_connect_t));
    if (connblock == NULL) return -1;

    connblock->block = (zv_connect_t*)(connblock + 1);
    connblock->next = NULL;
    blk->next = connblock;
    ++reactor->blkcnt;

    return 0;
}

zv_connect_t* zv_connect_get(zv_reactor_t *reactor, int fd)
{
    if (!reactor) return NULL;

    int blkidx = fd / EVENT_LENGTH;

    if (blkidx >= reactor->blkcnt) {
        zv_connblock_expand(reactor);
    }

    int i = 0;
    zv_connblock_t *blk = reactor->blockheader;
    while (i < blkidx) {
        blk = blk->next;
        ++i;
    }

    return &blk->block[fd % EVENT_LENGTH];
}
int readline(char *allbuf, int idx, char *linebuf)
{
    int len = strlen(allbuf);

    for (; idx < len; ++idx) {
        if (allbuf[idx] == '\r' && allbuf[idx+1] == '\n') {
            return idx + 2;
        } else {
            *(linebuf++) = allbuf[idx];
        }
    }

    return -1;
}

int zv_http_response(zv_connect_t *conn)
{
# if 0
    int len = sprintf(conn->wbuffer, 
                "HTTP/1.1 200 OK\r\n"
                "Accept-Ranges: bytes\r\n"
                "Content-Length: 121\r\n"
                "Content-Type: text/html\r\n"
                "Date: Sat, 06 Aug 2022 13:16:46 GMT\r\n\r\n"
                "<html><head><title>Only you</title></head><body><h1>Huang Anqi</h1><p>I miss you!</p><p>do you miss me?</p></body></html>"); 
    printf("data: %s, len: %d\n", conn->wbuffer, len);

    conn->wc = len;
#endif

    printf("http want to get resources: %s\n", conn->resource);

    int filefd = open(conn->resource, O_RDONLY);
    if (filefd == -1) {
        printf("open failed: %s\n", strerror(errno));
        return -1;
    }

    struct stat stat_buf;
    fstat(filefd, &stat_buf);
    close(filefd);

    /* encode http response header */
    int len = sprintf(conn->wbuffer, 
                "HTTP/1.1 200 OK\r\n"
                "Accept-Ranges: bytes\r\n"
                "Content-Length: %ld\r\n"
                "Content-Type: text/html\r\n"
                "Date: Sat, 06 Aug 2022 13:16:46 GMT\r\n\r\n", stat_buf.st_size); 
    printf("header: %s, len: %d\n", conn->wbuffer, len);
    conn->wc = len;

    conn->enable_sendfile = 1;

    return 0;
}

// GET
int zv_http_request(zv_connect_t *conn)
{
    printf("http --> request: \n %s\n", conn->rbuffer);
    
    char linebuffer[1024] = {0};
    int idx = readline(conn->rbuffer, 0, linebuffer);
    printf("line: %s\n", linebuffer);

    if (strstr(linebuffer, "GET")) {
        int i = 0;
        while (linebuffer[sizeof("GET ") + i] != ' ') {
            ++i;
        }

        linebuffer[sizeof("GET ") + i] = '\0';
        sprintf(conn->resource, "%s/%s", HTTP_WEB_ROOT, linebuffer + sizeof("GET "));
    }

    return 0;
}

int init_server(short port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(struct sockaddr_in));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) {
        printf("bind failed: %s\n", strerror(errno));
        return -1;
    }

    listen(sockfd, 10);
    printf("listen port: %d\n", port);
    return sockfd;
}

int set_listener(zv_reactor_t* reactor, int fd, ZVCALLBACK cb)
{
    if (!reactor || !reactor->blockheader) return -1;

    reactor->blockheader->block[fd].fd = fd;
    reactor->blockheader->block[fd].cb = cb;

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;

    epoll_ctl(reactor->epfd, EPOLL_CTL_ADD, fd, &ev);

    return 0;
}

int send_cb(int fd, int event, void *arg)
{
    zv_reactor_t* reactor = (zv_reactor_t*)arg;
    zv_connect_t *conn = zv_connect_get(reactor, fd);

    zv_http_response(conn);
    
    /*send http header*/
    send(fd, conn->wbuffer, conn->wc, 0);

    /* send http data*/
    if (conn->enable_sendfile) {
        int filefd = open(conn->resource, O_RDONLY);
        if (filefd == -1) {
            printf("open failed: %s\n", strerror(errno));
            return -1;
        }

        struct stat stat_buf;
        fstat(filefd, &stat_buf);

        int ret = sendfile(fd, filefd, NULL, stat_buf.st_size);
        if (ret == -1) {
            printf("send failed: %s\n", strerror(errno));
        }

        close(filefd);
    }

    conn->cb = recv_cb;

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(reactor->epfd, EPOLL_CTL_MOD, fd, &ev);

    return 0;
}

int recv_cb(int fd, int event, void *arg)
{
    zv_reactor_t *reactor = (zv_reactor_t*)arg;
    zv_connect_t *conn = zv_connect_get(reactor, fd);

    int ret = recv(fd, conn->rbuffer + conn->rc, conn->count, 0);
    if (ret < 0) {

    } else if (ret == 0) {
        conn->fd = -1;
        conn->rc = 0;
        conn->wc = 0;
        epoll_ctl(reactor->epfd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);

        return -1;
    } else {
        conn->rc += ret;
        // printf("rbuffer: %s, ret: %d\n", conn->rbuffer, conn->rc);

        zv_http_request(conn);
        conn->cb = send_cb;
        struct epoll_event ev;
        ev.events = EPOLLOUT;
        ev.data.fd = fd;
        epoll_ctl(reactor->epfd, EPOLL_CTL_MOD, fd, &ev);
    }
}

int accept_cb(int fd, int event, void *arg) {
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(struct sockaddr_in);

    int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);
    if (clientfd < 0) {
        printf("accept error: %s\n", strerror(errno));
        return -1;
    }

    printf("clientfd: %d\n", clientfd);

    zv_reactor_t *reactor = (zv_reactor_t*)arg;
    zv_connect_t *conn = zv_connect_get(reactor, clientfd);

    conn->fd = clientfd;
    conn->cb = recv_cb;
    conn->count = BUFFER_LENGTH;

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = clientfd;
    epoll_ctl(reactor->epfd, EPOLL_CTL_ADD, clientfd, &ev);
}

int main(int argc, char *argv[])
{
    if (argc < 2) return -1;

    zv_reactor_t reactor;
    zv_init_reactor(&reactor);

    int port = atoi(argv[1]);
    
    for (int i = 0; i < 1; ++i) {
        int sockfd = init_server(port + i);
        set_listener(&reactor, sockfd, accept_cb);
    }

    struct epoll_event events[EVENT_LENGTH] = {0};
    
    while (1) {
        int nready = epoll_wait(reactor.epfd, events, EVENT_LENGTH, -1);

        for (int i = 0; i < nready; ++i) {
            int connfd = events[i].data.fd;

            zv_connect_t *conn = zv_connect_get(&reactor, connfd);

            if (events[i].events & EPOLLIN) {
                conn->cb(connfd, events[i].events, &reactor);
            }

            if (events[i].events & EPOLLOUT) {
                conn->cb(connfd, events[i].events, &reactor);
            }
        }
    }

    
    return 0;
}

/*
EPOLL可读可写事件触发机制
1、水平触发：level trigger（LT）

读：只要缓冲区不为空就返回读就绪
写：只要缓冲区不满就返回写就绪

即：只要有数据就会触发，缓冲区剩余未读尽的数据会导致 epoll_wait() 返回

2、边缘触发：edge trigger（ET）

读：缓冲区由不可读变为可读（空->非空）、有新数据到达时、缓冲区有数据可读且使用 EPOLL_CTL_MODE 修改 EPOLLIN 事件时 
写：缓冲区由不可写变为可写（满->非满）、旧数据被送走时、缓冲区有空间可写且使用 EPOLL_CTL_MODE 修改 EPOLLOUT 事件时

即：只有数据到来才触发，不管缓冲区中是否有数据，缓冲区剩余未读尽的数据不会导致 epoll_wait() 返回
*/

/*
listen --> 触发accept_cb回调

EPOLLIN: accept_cb、recv_cb
EPOLLOUT: send_cb
*/