#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <memory.h>

#include<sys/epoll.h>
#include<fcntl.h>

#define SERVER_PORT 2118
#define BUFFER_SIZE 1024

void set_nonblock(int fd)  
{  
    int fl = fcntl(fd,F_GETFL);  
    fcntl(fd,F_SETFL,fl | O_NONBLOCK);  
}

int main(int argc, char* argv[])
{
    printf("socket...\n");
    int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == server_socket_fd){
        perror("Socket Error.\n");
        return 0;
    }
    int opt = 1;  
    setsockopt(server_socket_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    
    
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
    //create epoll max 256
    int epoll_fd = epoll_create(256);
    if(epoll_fd < 0){
        perror("epoll_create Error.\n");
        return 0;
    }
    
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_socket_fd;
    
    //monitor socket fd
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket_fd, &event);
    
    struct epoll_event recv_event[64];
    
    int timout = -1;
    int event_num = 0;
    
    while(1){
        event_num = epoll_wait(epoll_fd, recv_event, 64, timout);
        switch(event_num){
            case 0:
                //timed out
                printf("epoll timed out.\n");
                break;
            case -1:
                perror("epoll_wait error.");
                break;
            default:{
                    
                struct sockaddr_in client_socket;
                memset(&client_socket,0x00,sizeof(client_socket));
                int client_len = sizeof(struct sockaddr_in);
                int i = 0;
                for(i = 0; i < event_num; i++){
                    int recv_sock_fd = recv_event[i].data.fd;
                    if(recv_sock_fd == server_socket_fd 
                        &&((recv_event[i].events) & EPOLLIN)){
                       //new client coming
                        int client_fd = accept(server_socket_fd,
                                               (struct sockaddr *)&client_socket,&client_len);
                        printf("server:new client %d \n", client_fd);
                        set_nonblock(client_fd);
                        struct epoll_event event;
                        event.events = EPOLLIN | EPOLLET;
                        event.data.fd = client_fd;
                        
                        //monitor socket fd
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
                    }else if(recv_event[i].events & EPOLLIN){
                        char buffer[BUFFER_SIZE];
                        memset(buffer, 0, sizeof(buffer));
                        int len = recv(recv_sock_fd, buffer, sizeof(buffer), 0);
                        printf("server: recv client=%d len=%d\n",recv_sock_fd,len);
                        if(len > 0){
                                if(strcmp(buffer, "exit\n") == 0){
                                    break;
                                }
                                printf("server: client %d , msg:%s\n",recv_sock_fd,buffer);
                                set_nonblock(recv_sock_fd);
                                struct epoll_event event;
                                event.data.fd = recv_sock_fd;
                                event.events = EPOLLOUT | EPOLLET;
                                //修改标识符，等待下一个循环时发送数据，异步处理的精髓  
                                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, recv_sock_fd, &event);
                        }else if(len == 0){
                                //client close
                                epoll_ctl(epoll_fd,EPOLL_CTL_DEL,recv_sock_fd,NULL);
                                printf("server: client %d close.\n",recv_sock_fd);  
                                close(recv_sock_fd);
                        }else if(len == -1){
                            epoll_ctl(epoll_fd,EPOLL_CTL_DEL,recv_sock_fd,NULL);
                            close(recv_sock_fd);
                            printf("server: client %d close.\n",recv_sock_fd);
                        }

                    }else if(recv_event[i].events & EPOLLOUT){
                        const char *msg = "Hello, you are client!\n";  
                        send(recv_sock_fd, msg, strlen(msg), 0);
                        printf("server: send to client %d .\n",recv_sock_fd);
                        set_nonblock(recv_sock_fd);
                        struct epoll_event event;
                        event.data.fd = recv_sock_fd;
                        event.events = EPOLLIN | EPOLLET;
                        //修改标识符，等待下一个循环时接收数据 
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, recv_sock_fd, &event);
                    }
                }
                
                break;
            }
        }
    }
    
    //close(server_socket_fd);
    return 0;
}
