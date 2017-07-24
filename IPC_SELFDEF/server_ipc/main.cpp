#include <unistd.h>
#include <stdio.h>
#include <utils/RefBase.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>

struct xyf_write_read{
    char name[10];
    size_t len;
    int out_handle;//from kernel to user space
    int in_handle; //from user space to kernel
    int data;
    int pid;
};

#define REGISTER_SERVICE _IOWR('x', 11, struct xyf_write_read)
#define GET_SERVICE _IOWR('x', 12, struct xyf_write_read)
#define SERVER_ENTER_LOOP _IOWR('x', 13, struct xyf_write_read)
#define SERVER_REPLY _IOWR('x', 14, struct xyf_write_read)
#define CLIENT_REQUEST _IOWR('x', 15, struct xyf_write_read)

static int register_as_server(){
    int fd = open("/dev/xyf", O_RDWR | O_CLOEXEC);
    printf("open_driver /dev/xyf fd=%d \n", fd);
    if(fd >= 0){
         char name[10]="a_service";
         struct xyf_write_read xyf;
         memset(&xyf, 0x00, sizeof(struct xyf_write_read));
         memcpy(&xyf.name, name, 10);
         xyf.len = 9;
         int result = ioctl(fd, REGISTER_SERVICE, &xyf);
         printf("register %s, result=%d\n", name, result);
    }else{
        printf("Opening '/dev/xyf' failed: %s\n", strerror(errno));
    }
    return fd;
}

static void listening(int fd){
    struct xyf_write_read xyf_data;
    int result = ioctl(fd, SERVER_ENTER_LOOP, &xyf_data);
    printf("a_service receive data=%d \n", xyf_data.data);
    xyf_data.in_handle = xyf_data.out_handle;
    xyf_data.data = xyf_data.data + 10;
    result = ioctl(fd, SERVER_REPLY, &xyf_data);
    close(fd);
}

int main() {
    int fd = register_as_server();
    if(fd >= 0){
        listening(fd);
    }
    return 0;
}



