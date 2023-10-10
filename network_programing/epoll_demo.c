/*
 * tcp server: multi-io: epoll
 */
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>

#define LISTEN_PORT 30064
#define MAX_LISTEN_NUM 10
#define BUFF_LEN    1024

int main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("create socket failed. error: %s\n", strerror(errno));
        return -1;
    }

    // 设置server地址
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);   // INADDR_ANY: 0.0.0.0
    servaddr.sin_port = htons(LISTEN_PORT);

    if (-1 == bind(sockfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr))) {
        printf("bind failed. error: %s\n", strerror(errno));
        return -1;
    }

    listen(sockfd, MAX_LISTEN_NUM);

    struct sockaddr_in clientaddr;
    socklen_t client_len = sizeof(clientaddr);

    int epfd = epoll_create(1);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;

    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    struct epoll_event events[1024] = {0};  // 记录就绪事件

    while (1) {
        int nready = epoll_wait(epfd, events, 1024, -1);
        if (nready < 0) continue;

        for (int i = 0; i < nready; i++) {
            int connfd = events[i].data.fd;

            if (sockfd == connfd) { // 建立连接
                int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &client_len);
                if (clientfd < 0) {
                    continue;
                }

                printf("clientfd: %d\n", clientfd);

                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = clientfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);
            } else if (events[i].events & EPOLLIN) {  // 可读
                char buffer[10] = {0};
                short len = 0;
                int rbytes = recv(connfd, buffer, 5, 0);  // 第一次只读5个字节
                if (rbytes > 0) {
                    printf("recv data from clientfd[%d]: %s, recv bytes: %d\n", connfd, buffer, rbytes);
                    send(connfd, buffer, rbytes, 0);
                } else if (rbytes == 0) {
                    printf("close\n");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, NULL);
                    close(connfd);
                }
                
            }
        }

    }
    
    return 0;
}


/*
ET(边缘触发)：
 只会唤醒服务器一次，去读取缓冲区，如果缓冲区中还剩余未读取完的数据，会在下一次触发可读事件时读取

LT(水平触发)：
 只要缓冲区存在可读数据，会不断触发可读事件，唤醒服务器去读数据，直到读完
*/