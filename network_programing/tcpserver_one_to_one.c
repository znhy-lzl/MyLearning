/*
 * tcp server: one server accept one connection from client
 */
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

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

    int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &client_len);
    printf("accpet connection from client %d, the client address is %s, port is %d\n", 
           clientfd, inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);

    char buffer[BUFF_LEN] = {0};
    int recvlen = recv(clientfd, buffer, BUFF_LEN, 0);
    printf("recv data: %s, data len = %d\n", buffer, recvlen);

    send(clientfd, buffer, recvlen, 0);

    return 0;
    
    
}
