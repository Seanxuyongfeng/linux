#include <unistd.h>
#include <stdio.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <utils/RefBase.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>

struct xyf_transaction {
    int pid;
    int data;
};

#define SET_AS_SERVER _IOW('x', 7, __s32)
#define SERVER_ENTER_LOOP _IOWR('x', 8, struct xyf_transaction)
#define SERVER_REPLY _IOWR('x', 9, struct xyf_transaction)

static int open_driver()
{
    int fd = open("/dev/xyf", O_RDWR | O_CLOEXEC);
    printf("open_driver server fd=%d \n", fd);
    if(fd >= 0){
        int result = 0;
        
        ioctl(fd, SET_AS_SERVER);
        
        do{
            struct xyf_transaction xyf;
            result = ioctl(fd, SERVER_ENTER_LOOP, &xyf);
            printf("ioctl server receive pid=%d,data=%d \n", xyf.pid,xyf.data);
            xyf.data = xyf.data + 10;
            printf("ioctl server response =%d\n", xyf.data);
            result = ioctl(fd, SERVER_REPLY, &xyf);
            printf("ioctl end %d\n", result);
        }while(result != -1);
            close(fd);
    }else{
        printf("Opening '/dev/xyf' failed: %s\n", strerror(errno));
    }
    return fd;
}
int main() {
    using namespace android;
    open_driver();
    sp<ProcessState> ps(ProcessState::self());
    ps->startThreadPool();
    ps->giveThreadPoolName();
    IPCThreadState::self()->joinThreadPool();
    return 0;
}



