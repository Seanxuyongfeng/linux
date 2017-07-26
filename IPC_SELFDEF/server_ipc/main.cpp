#include <unistd.h>
#include <stdio.h>
#include <utils/RefBase.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

struct xyf_write_read{
    char name[10];
    size_t len;
    int out_handle;//from kernel to user space
    int in_handle; //from user space to kernel
    int data;
    int pid;
};
static int mDriverFD = 0;
int handle_service_a = -1;
int handle_service_b = -1;

#define REGISTER_SERVICE _IOWR('x', 11, struct xyf_write_read)
#define GET_SERVICE _IOWR('x', 12, struct xyf_write_read)
#define SERVER_ENTER_LOOP _IOWR('x', 13, struct xyf_write_read)
#define SERVER_REPLY _IOWR('x', 14, struct xyf_write_read)
#define CLIENT_REQUEST _IOWR('x', 15, struct xyf_write_read)


static void listening(int fd){
    struct xyf_write_read xyf_data;
    int result = ioctl(fd, SERVER_ENTER_LOOP, &xyf_data);
    printf("a_service receive data=%d \n", xyf_data.data);
    xyf_data.in_handle = xyf_data.out_handle;
    char a_service[10]="a_service";
    char b_service[10]="b_service";
    if(handle_service_a == xyf_data.out_handle){
        printf("request a_service\n");
        xyf_data.data = xyf_data.data + 10;
    }else if(handle_service_b == xyf_data.out_handle){
        printf("request b_service\n");
        xyf_data.data = xyf_data.data + 20;
    }else{
        printf("server no such a service %d\n", xyf_data.out_handle);
    }
    
    result = ioctl(fd, SERVER_REPLY, &xyf_data);
}

static void* loop_one(void *arg){
    pthread_detach(pthread_self());
    while(1){
        listening(mDriverFD);
    }
    
    return NULL;
}

static void* loop_two(void *arg){
    pthread_detach(pthread_self());
    while(1){
        listening(mDriverFD);
    }
    return NULL;
}

static void* loop_thr(void *arg){
    pthread_detach(pthread_self());
    while(1){
        listening(mDriverFD);
    }
    return NULL;
}

static void prepare_thread1(){
    pthread_t thread;
    int result = pthread_create(&thread, NULL, loop_one, NULL);
    if(result){
        printf("Unable to create thread one result=%d\n", result);
    }
}

static void prepare_thread2(){
    pthread_t thread;
    int result = pthread_create(&thread, NULL, loop_two, NULL);
    if(result){
        printf("Unable to create thread one result=%d\n", result);
    }
}

static void prepare_thread3(){
    pthread_t thread;
    int result = pthread_create(&thread, NULL, loop_thr, NULL);
    if(result){
        printf("Unable to create thread one result=%d\n", result);
    }
}

static void papare_looper(){
    prepare_thread1();
    prepare_thread2();
    prepare_thread3();
}

static int open_driver(){
    int fd = open("/dev/xyf", O_RDWR | O_CLOEXEC);
    printf("open_driver /dev/xyf fd=%d \n", fd);
    if(fd >= 0){
        return fd;
    }else{
        printf("Opening '/dev/xyf' failed: %s\n", strerror(errno));
    }
    return fd;
}

static void register_a_service(int fd){
    if(fd >= 0){
         char name[10]="a_service";
         struct xyf_write_read xyf;
         memset(&xyf, 0x00, sizeof(struct xyf_write_read));
         memcpy(&xyf.name, name, 10);
         xyf.len = 9;
         int result = ioctl(fd, REGISTER_SERVICE, &xyf);
         handle_service_a = xyf.out_handle;
         printf("register %s, handle=%d\n", name, handle_service_a);
    }else{
        printf("Opening '/dev/xyf' failed: %s\n", strerror(errno));
    }
}

static void register_b_service(int fd){
    if(fd >= 0){
         char name[10]="b_service";
         struct xyf_write_read xyf;
         memset(&xyf, 0x00, sizeof(struct xyf_write_read));
         memcpy(&xyf.name, name, 10);
         xyf.len = 9;
         int result = ioctl(fd, REGISTER_SERVICE, &xyf);
         handle_service_b = xyf.out_handle;
         printf("register %s, handle=%d\n", name, handle_service_b);
    }else{
        printf("Opening '/dev/xyf' failed: %s\n", strerror(errno));
    }
}

int main() {
    int fd = open_driver();
    mDriverFD = fd;
    if(fd >= 0){
        register_a_service(fd);
        register_b_service(fd);
        papare_looper();
    }
    while(1){
        sleep(5);
    }
    return 0;
}



