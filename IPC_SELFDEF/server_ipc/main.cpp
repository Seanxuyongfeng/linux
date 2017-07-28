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
#define BC_REQUEST_DEATH_NOTIFICATION _IOWR('x', 16, struct xyf_write_read) 
#define PROC_ENTER_LOOP _IOWR('x', 17, struct xyf_write_read) 

static void listening(int fd){
    struct xyf_write_read xyf_data;
    int result = ioctl(fd, SERVER_ENTER_LOOP, &xyf_data);
    printf("a_service receive data=%d \n", xyf_data.data);
    xyf_data.in_handle = xyf_data.out_handle;
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

static int get_c_service_handle(int fd){
    struct xyf_write_read xyf_data;
    char a_service[10]="c_service";
    memset(&xyf_data, 0x00, sizeof(struct xyf_write_read));
    memcpy(&xyf_data.name, a_service, 10);
    xyf_data.len = 9;
    ioctl(fd, GET_SERVICE, &xyf_data);
    printf("get service handle=%d\n", xyf_data.out_handle);
    return xyf_data.out_handle;
}

static void calc(int fd, int handle){
    struct xyf_write_read xyf_data;
    memset(&xyf_data, 0x00, sizeof(struct xyf_write_read));
    xyf_data.in_handle = handle;
    xyf_data.data = 76;
    ioctl(fd, CLIENT_REQUEST, &xyf_data);
    printf("get reply =%d \n", xyf_data.data);
}

static void linkToDeath(int fd, int target_fd){
    struct xyf_write_read xyf_data;
    memset(&xyf_data, 0x00, sizeof(struct xyf_write_read));
    xyf_data.in_handle = target_fd;
    ioctl(fd, BC_REQUEST_DEATH_NOTIFICATION, &xyf_data);
    printf("listering service %d \n", target_fd);
}

static void main_loop(int fd){
    struct xyf_write_read xyf_data;
    memset(&xyf_data, 0x00, sizeof(struct xyf_write_read));
    int result = ioctl(fd, PROC_ENTER_LOOP, &xyf_data);
    int death_handle = xyf_data.out_handle;
    printf("target service %d died\n", death_handle);
}

int main() {
    int fd = open_driver();
    mDriverFD = fd;
    if(fd >= 0){
        register_a_service(fd);
        register_b_service(fd);
        papare_looper();
    }
    //waiting service c to register
    sleep(5);
    
    int handle = get_c_service_handle(fd);
    calc(fd, handle);

    linkToDeath(mDriverFD, handle);
    while(1){
        main_loop(mDriverFD);
    }
    return 0;
}



