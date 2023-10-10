#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>

#define SERVER_PORT         8080
#define SERVER_IP           "127.0.0.1"
#define MAX_BUFFER          128
#define MAX_EPOLLSIZE       100000
#define MAX_PORT            100
#define CPU_CORES_SIZE      8

#define TIME_SUB_MS(tv_cur, tv_begin)  ((tv_cur.tv_sec - tv_begin.tv_sec) * 1000 + (tv_cur.tv_usec - tv_begin.tv_usec) / 1000)

static int curfds = 1;
static int nRun = 0;

static int ntySetNonBlock(int fd)
{
    int flags;
    flags = fcntl(fd, F_GETFL, 0);    /*need get the file status firstly */
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, 0) < 0) {  /* set fail */
        return -1;
    }

    return 0;
}

static int ntySetReUseAddr(int fd)
{
    int reuse = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
}

int listenfd(int fd, int *fds)
{
    for (int i = 0; i < MAX_PORT; ++i) {
        if (fd == *(fds + i)) {
            return *(fds + i);
        }
    }

    return 0;
}

int main(void)
{
    int sockfds[MAX_PORT] = {0};
    printf("C1000K Server Start\n");

    int epoll_fd = epoll_create(MAX_EPOLLSIZE);

    for (int i = 0; i < MAX_PORT; ++i) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket failed");
            return 1;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(SERVER_PORT + i);

        if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
            perror("bind failed");
            return 2;
        }

        if (listen(sockfd, 5) < 0) {
            perror("listen");
            return 3;
        }

        sockfds[i] = sockfd;
        printf("C1000K Server Listen on Port: %d\n", SERVER_PORT + i);

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = sockfd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev);
    }

    struct timeval tv_cur;
    gettimeofday(&tv_cur, NULL);

    struct epoll_event events[MAX_EPOLLSIZE];

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, curfds, 5);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int sockfd = listenfd(events[i].data.fd, sockfds);
            if (sockfd) {
                struct sockaddr_in client_addr;
                memset(&client_addr, 0, sizeof(struct sockaddr_in));
                socklen_t client_len = sizeof(client_addr);

                int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
                if (clientfd < 0) {
                    perror("accept");
                    return 4;
                }

                if (curfds++ > 1000 * 1000) {
                    nRun = 1;
                }

                if (curfds % 1000 == 999) {
                    struct timeval tv_begin;
                    memcpy(&tv_begin, &tv_cur, sizeof(struct timeval));
                    gettimeofday(&tv_cur, NULL);
                    
                    int time_used = TIME_SUB_MS(tv_cur, tv_begin);
                    printf("connections: %d, sockfd: %d, time_used: %d\n", curfds, clientfd, time_used);
                }

                ntySetNonBlock(clientfd);
                ntySetReUseAddr(clientfd);
                
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                ev.data.fd = clientfd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clientfd, &ev);
            } else {

            }
        }
    }
    return 0;
}