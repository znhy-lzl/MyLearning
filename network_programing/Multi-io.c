/*
 * tcp server: multi-io: select and poll
 */
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
//#include <sys/select.h>
#include <unistd.h>
#include <poll.h>

#define LISTEN_PORT 30064
#define MAX_LISTEN_NUM 10
#define BUFF_LEN    1024

#define POLL_SIZE 1000

//#define CHOOSE_SELECT

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


#ifdef CHOOSE_SELECT
    fd_set rfds, rset;   // 记录可读的文件描述符
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);  // 把sockfd设置到可读集合中，当sockfd可读时，响应

    int maxfd = sockfd;
    int clientfd = 0;
    
    while(1) {
        rset = rfds;     // 重置rset
        int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(sockfd, &rset)) {  // 当sockfd可读时，进行处理
            clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &client_len);
                printf("accpet connection from client %d, the client address is %s, port is %d\n", 
                clientfd, inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);

            FD_SET(clientfd, &rfds);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }

            if (--nready == 0) {
                continue;
            }
        }

        int i = 0;
        for (i = sockfd + 1; i <= maxfd; ++i) {  // server接收来自每一个客户端的数据
            if (FD_ISSET(i, &rset)) {
                char buffer[BUFF_LEN] = {0};
                int recvlen = recv(i, buffer, BUFF_LEN, 0);
                if (recvlen == 0) {
                    close(i);
                    break;
                }
                printf("recv data: %s, data len = %d\n", buffer, recvlen);

                send(i, buffer, recvlen, 0);
            }
        }
    
    }


#else // CHOOSE_POLL
    struct pollfd fds[POLL_SIZE] = {0};
    fds[sockfd].fd = sockfd;
    fds[sockfd].events = POLLIN;

    int maxfd = sockfd;
    int clientfd = 0;

    while (1) {
        int nready = poll(fds, maxfd + 1, -1);
        if (fds[sockfd].revents & POLLIN) {
            clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &client_len);
            printf("accpet connection from client %d, the client address is %s, port is %d\n", 
                clientfd, inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);

            fds[clientfd].fd = clientfd;
            fds[clientfd].events = POLLIN;

            if (clientfd > maxfd) {
                maxfd = clientfd;
            }

            if (--nready == 0) {
                continue;
            }
        }

        int i = 0;
        for (i = sockfd + 1; i <= maxfd; ++i) {
            if (fds[i].revents & POLLIN) {
                char buffer[BUFF_LEN] = {0};
                int recvlen = recv(i, buffer, BUFF_LEN, 0);
                if (recvlen == 0) {
                    fds[i].fd = -1;
                    fds[i].events = 0;

                    close(i);
                    break;
                }
                printf("recv data: %s, data len = %d\n", buffer, recvlen);

                send(i, buffer, recvlen, 0);
            }
        }
    }
#endif
    return 0;
    
    
}
