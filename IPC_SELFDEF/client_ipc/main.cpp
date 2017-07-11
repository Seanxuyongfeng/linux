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

#define CLIENT_REQUEST _IOWR('x', 10, struct xyf_transaction)

static int open_driver()
{
    int fd = open("/dev/xyf", O_RDWR | O_CLOEXEC);
    printf("open_driver client fd=%d \n", fd);
    if(fd >= 0){
        int result = 0;

        struct xyf_transaction xyf;
        xyf.data = 56;
        result = ioctl(fd, CLIENT_REQUEST, &xyf);
        printf("ioctl receive from server pid=%d result=%d\n", xyf.pid, xyf.data);
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
    //IPCThreadState::self()->joinThreadPool();
    return 0;
}



