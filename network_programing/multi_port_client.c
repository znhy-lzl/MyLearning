#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>

#define MAX_BUFFER           128
#define MAX_EPOLLSIZE        (284 * 1024)
#define MAX_PORT             100    /* 1 */

int isContinue = 0;

#define TIME_SUB_MS(tv_cur, tv_begin)  ((tv_cur.tv_sec - tv_begin.tv_sec) * 1000 + (tv_cur.tv_usec - tv_begin.tv_usec) / 1000)

static int ntySetNonBlock(int fd)
{
    int flags;
    flags = fcntl(fd, F_GETFL, 0);    /*need get the file status firstly */
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {  /* set fail */
        return -1;
    }

    return 0;
}

static int ntySetReUseAddr(int fd)
{
    int reuse = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
}

int main(int argc, char **argv)
{
    if (argc <= 2) {
        printf("Usage: %s ip port", argv[0]);
        exit(0);
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int connections = 0;
    char buffer[128] = {0};
    int index = 0;

    struct epoll_event events[MAX_EPOLLSIZE];
    int epoll_fd = epoll_create(MAX_EPOLLSIZE);
    strcpy(buffer, "Data From MulClient\n");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);

    struct timeval tv_cur;
    gettimeofday(&tv_cur, NULL);

    while (1) {
        if (++index >= MAX_PORT) {
            index = 0;
        }

        struct epoll_event ev;
        int sockfd = 0;

        if (connections < 380000 && !isContinue) {
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1) {
                printf("socket error: %s\n", strerror(errno));
                return -1;
            }

            addr.sin_port = htons(port + index);

            if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
                printf("connect error: %s\n", strerror(errno));
                return -1;
            }

            ntySetNonBlock(sockfd);
            ntySetReUseAddr(sockfd);

            sprintf(buffer, "Hello Server: client --> %d\n", connections);
            send(sockfd, buffer, strlen(buffer), 0);

            ev.data.fd = sockfd;
            ev.events = EPOLLIN | EPOLLOUT;
            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev);

            ++connections;
        }

        if (connections % 1000 == 999 || connections >= 380000) {
            struct timeval tv_begin;
            memcpy(&tv_begin, &tv_cur, sizeof(struct timeval));
            gettimeofday(&tv_cur, NULL);

            int time_used = TIME_SUB_MS(tv_cur, tv_begin);
            printf("connections: %d, sockfd:%d, time_used:%d\n", connections, sockfd, time_used);
            
            int nfds = epoll_wait(epoll_fd, events, connections, 100);
            for (int i = 0; i < nfds; ++i) {
                int clientfd = events[i].data.fd;

                if (events[i].events & EPOLLOUT) {
                    sprintf(buffer, "data from %d\n", clientfd);
                    send(sockfd, buffer, strlen(buffer), 0);
                } else if (events[i].events & EPOLLIN) {
                    char rBuffer[MAX_BUFFER] = {0};
                    ssize_t length = recv(sockfd, rBuffer, MAX_BUFFER, 0);
                    if (length > 0) {
                        printf("RecvBuffer: %s\n", rBuffer);

                        if (!strcmp(rBuffer, "quit")) {
                            isContinue = 0;
                        }
                    } else if (length == 0) {
                        printf("Disconnect clientfd: %d\n", clientfd);
                        connections--;
                        close(clientfd);
                    } else {
                        if (errno == EINTR) {
                            continue;
                        }

                        printf("Error clientfd: %d, error: %s\n", clientfd, strerror(errno));
                        close(clientfd);
                    }
                } else {
                    printf("clientfd: %d, error: %s\n", clientfd, strerror(errno));
                    close(clientfd);
                }
            }
        }

        usleep(1 * 1000);

    }

    return 0;
}


