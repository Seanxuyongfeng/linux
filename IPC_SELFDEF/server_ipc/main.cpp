#include <unistd.h>
#include <stdio.h>
#include <utils/RefBase.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>

struct transaction_data{
    int handle;
    int data;
    int pid;
};

//user to send data to kernel
struct xyf_write_read{
    char name[10];
    size_t len;
    int in_handle; //from user space to kernel
    int data;//from user space to kernel
    int pid;//from user space to kernel
    //user space can read from this buffer which created by kernel
    unsigned long read_buffer;
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
    int service_handle = -1;
    int receive_data = -1;
    struct transaction_data * data = (struct transaction_data *)(xyf_data.read_buffer);
    receive_data = data->data;
    service_handle = data->handle;
    
    printf("a_service receive data=%d \n", receive_data);
    
    if(handle_service_a == service_handle){
        printf("request a_service\n");
        xyf_data.data = receive_data + 10;
    }else if(handle_service_b == service_handle){
        printf("request b_service\n");
        xyf_data.data = receive_data + 20;
    }else{
        printf("server no such a service %d\n", service_handle);
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

#define BINDER_VM_SIZE ((1*1024*1024) - (4096 *2))

static int mmap(int fd){
    if(mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0) == MAP_FAILED){
        printf("mmap failed!!! \n");
    }
    return 0;
}

static void register_a_service(int fd){
    if(fd >= 0){
         char name[10]="a_service";
         struct xyf_write_read xyf;
         memset(&xyf, 0x00, sizeof(struct xyf_write_read));
         memcpy(&xyf.name, name, 10);
         xyf.len = 9;
         int result = ioctl(fd, REGISTER_SERVICE, &xyf);
         struct transaction_data * data = (struct transaction_data *)(xyf.read_buffer);
         handle_service_a = data->handle;
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
         if(xyf.read_buffer != 0){
             struct transaction_data * data = (struct transaction_data *)(xyf.read_buffer);
             handle_service_b = data->handle;
             printf("registerbbb %s, handle=%d\n", name, data->handle);
         }
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
    
    int service_c_handle = -1;
    struct transaction_data * data = (struct transaction_data *)(xyf_data.read_buffer);
    service_c_handle = data->handle;
    printf("get service c handle=%d\n", service_c_handle);
    return service_c_handle;
}

static void calc(int fd, int handle){
    struct xyf_write_read xyf_data;
    memset(&xyf_data, 0x00, sizeof(struct xyf_write_read));
    xyf_data.in_handle = handle;
    xyf_data.data = 76;
    ioctl(fd, CLIENT_REQUEST, &xyf_data);
    
    int receive_data = -1;
    struct transaction_data * data = (struct transaction_data *)(xyf_data.read_buffer);
    receive_data = data->data;
    
    printf("get reply from service c =%d \n", receive_data);
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
    
    struct transaction_data * data = (struct transaction_data *)(xyf_data.read_buffer);
        
    int death_handle = data->handle;
    printf("target service %d died\n", death_handle);
}

int main() {
    int fd = open_driver();
    mDriverFD = fd;
    if(fd >= 0){
        mmap(fd);
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



