#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <memory.h>

#define SERVER_PORT 2118
#define BUFFER_SIZE 1024

int main(int argc, char* argv[])
{
    printf("socket...\n");
    int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == server_socket_fd){
        perror("Socket Error.\n");
        return 0;
    }
    setnonblocking(server_socket_fd);
    struct sockaddr_in socket_server_in;
    memset(&socket_server_in,0x00,sizeof(socket_server_in));
    socket_server_in.sin_family = AF_INET;
    socket_server_in.sin_port = htons(SERVER_PORT);
    socket_server_in.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("bind...\n");
    if(bind(server_socket_fd, (struct sockaddr*)&socket_server_in,sizeof(struct sockaddr)) == -1){
        perror("Bind Error.\n");
        return 0;
    }
    printf("listen...\n");
    if(listen(server_socket_fd, 15) == -1){
        perror("Listen Error.\n");
        return 0;
    }
    struct sockaddr_in client_socket;
    //memset(&client_socket,0x00,sizeof(client_socket));
    int client_len = sizeof(struct sockaddr_in);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    while(1){
        printf("Server wait...\n");
        int client_fd = accept(server_socket_fd,
                    (struct sockaddr *)&client_socket,&client_len);
        if(client_fd == -1){
            perror("Accept Error.\n");
            continue;
        }
        while(1){
            memset(buffer, 0, sizeof(buffer));
            int len  = recv(client_fd, buffer, sizeof(buffer), 0);
            if(strcmp(buffer, "exit\n") == 0){
                break;
            }
            printf("server receive:%s\n",buffer);
            send(client_fd, buffer, len, 0);
        }
        close(client_fd);
    }
    close(server_socket_fd);
    return 0;
}
