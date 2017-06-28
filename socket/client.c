#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <memory.h>

#define SERVER_PORT 2118
#define BUFFER_SIZE 1024

int main(int argc, char* argv[])
{
    int socket_client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_client_fd == -1){
        perror("Socket Error.");
        return 0;
    }
    
    struct sockaddr_in socket_server_in;
    memset(&socket_server_in,0x00, sizeof(socket_server_in));
    socket_server_in.sin_family = AF_INET;
    socket_server_in.sin_port = htons(SERVER_PORT);
    socket_server_in.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if(connect(socket_client_fd,(struct sockaddr*)&socket_server_in,
                    sizeof(struct sockaddr)) == -1){
        perror("Connect Error.");
        return 0;
    }
    char sendbuf[BUFFER_SIZE];
    memset(sendbuf, 0x00,sizeof(sendbuf));
    char buff[BUFFER_SIZE];
    
    while (fgets(sendbuf, sizeof(sendbuf), stdin) != NULL){
        send(socket_client_fd, sendbuf, strlen(sendbuf),0); 
        memset(buff, 0x00, sizeof(buff));
        if(recv(socket_client_fd, buff, sizeof(buff), 0) == -1){
            perror("Read Error.");
        }
        printf("client receive:%s\n", buff);
    }
    
    close(socket_client_fd);
    return 0; 
}
