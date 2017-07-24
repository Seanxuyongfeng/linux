#include "xyf.h"
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/freezer.h>
#include <linux/mm.h>
#include <linux/nsproxy.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/pid_namespace.h>
#include <linux/security.h>

struct svcinfo
{
    struct svcinfo *next;
    int handle;
    size_t len;
    char name[10];
};

struct svcinfo *svclist = NULL;
struct svcinfo *find_svc(char *name, size_t len){
    struct svcinfo *si;
    for(si = svclist; si; si = si->next){
        if((len == si->len) && !memcmp(name, si->name, len*sizeof(char))){
            return si;
        }
    }
    return NULL;
}

int add_service(char *name, int handle,size_t len){
    struct svcinfo *si;
    
    si = (struct svcinfo *)kzalloc(sizeof(*si),GFP_KERNEL);
    si->handle = handle;
    si->len = len;
    memcpy(si->name, name, 10*sizeof(char));
    si->next = svclist;
    svclist = si;
    return 0;
}

//trasaction between client and server
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

//notify and save data
struct binder_proc
{
    struct hlist_node list_node;
    int pid;
    int handle;//self
    int target;
    int data;
    int target_pid;
    wait_queue_head_t wait;
};

static HLIST_HEAD(xyf_list);//struct hlist_head

static struct binder_proc * find_target(int handle){
    struct binder_proc *item;
    hlist_for_each_entry(item, &xyf_list, list_node){
        if(item->handle == handle){
            return item;
        }
    }
    return NULL;
}

static int generate_handle(void){
    int handle;
    int count;
    struct binder_proc *item;
    
    handle = 0;
    count = 0;
    
    hlist_for_each_entry(item, &xyf_list, list_node){
        count++;
    }
    if(count > 1){
        handle = count - 1;
    }
    return handle;
}

static int notify_target(struct binder_proc *source,struct xyf_write_read *bwr){
    struct binder_proc *target = find_target(bwr->in_handle);
    if(target == NULL){
        printk("xyf_ioctl cannot find target %d.\n",bwr->in_handle);
        return -1;
    }
    //put data to server buffer, can user other ways
    target->data = bwr->data;
            
    target->target = source->handle;
    target->target_pid = source->pid;
    wake_up_interruptible(&target->wait);
    return 0;
}

static int get_handle(struct xyf_write_read *bwr){
    struct svcinfo * service = find_svc(bwr->name, bwr->len);
    if(service != NULL){
        return service->handle;
    }else{
        printk("xyf_ioctl cannot find handle %s\n", bwr->name);
        return -1;
    }
}

static long xyf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    int result, handle;
    struct xyf_write_read bwr;
    void __user *ubuf = (void __user *)arg;
    struct binder_proc *device = filp->private_data;
    
    printk("xyf_ioctl cmd: %d\n", cmd);

    switch (cmd) {
        case SERVER_ENTER_LOOP:{//server
            struct xyf_write_read __user *reply = ubuf;
            device->data = 0;
            
            printk("xyf_ioctl server enter loop...\n");
            if (wait_event_interruptible(device->wait, (device->data != 0))){
                return 0;
            }
            printk("xyf_ioctl server get data from client %d\n", device->data);
            if (put_user(device->data, &reply->data)) {
                return -EINVAL;
            }
            if (put_user(device->target_pid, &reply->pid)) {
                return -EINVAL;
            }
            if (put_user(device->target, &reply->out_handle)) {
                return -EINVAL;
            }
            return 0;
        }
        case CLIENT_REQUEST:{//client request
            struct xyf_write_read __user *reply = ubuf;
            device->data = 0;
            
            if (copy_from_user(&bwr, ubuf, sizeof(struct xyf_write_read))){
                return -EFAULT;
            }
            result = notify_target(device, &bwr);
            if(result == -1){
                return 0;
            }
            //wait for server response
            printk("xyf_ioctl client waiting on %d.\n", device->handle);
            if (wait_event_interruptible(device->wait, (device->data != 0))){
                return 0;
            }
            
            printk("xyf_ioctl client response from server %d.\n", device->data);

            if (put_user(device->data, &reply->data)) {
                return -EINVAL;
            }
            if (put_user(device->target_pid, &reply->pid)) {
                return -EINVAL;
            }
            return 0;
        }
        case SERVER_REPLY:{//server response
            printk("xyf_ioctl server response.\n");
            if (copy_from_user(&bwr, ubuf, sizeof(struct xyf_write_read))){
                return -EFAULT;
            }

            result = notify_target(device, &bwr);
            if(result == -1){
                return 0;
            }
            return 0;
        }
        case REGISTER_SERVICE:{
            if (copy_from_user(&bwr, ubuf, 
                        sizeof(struct xyf_write_read))){
                return -EFAULT;
            }
            device->handle = generate_handle();
            device->data = 0;
            add_service(bwr.name, 
                        device->handle,
                        bwr.len);
            
            printk("xyf_ioctl add service %s, len = %d\n", 
                   bwr.name, (int)(bwr.len));
            return 0;
        }
        case GET_SERVICE:{
            struct xyf_write_read __user *reply = ubuf;
            
            if (copy_from_user(&bwr, ubuf, sizeof(struct xyf_write_read))){
                return -EFAULT;
            }
            handle = get_handle(&bwr);
            if (put_user(handle, &reply->out_handle)) {
                return -EINVAL;
            }
            return 0;
        }
        default:
            break;
    }
    return 0;
}

static int xyf_open(struct inode *nodp, struct file *filp)
{
    struct binder_proc *device;

    device = (struct binder_proc *)kzalloc(sizeof(*device), GFP_KERNEL);
    if (device == NULL){
        printk("xyf_open kzalloc failed.\n");
        return -ENOMEM;
    }
    init_waitqueue_head(&device->wait);
    hlist_add_head(&device->list_node, &xyf_list);
    device->pid = current->pid;
    filp->private_data = device;
    printk("xyf_open %d.\n", device->pid);
    return 0;
}

static const struct file_operations xyf_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = xyf_ioctl,
	.compat_ioctl = xyf_ioctl,
	.open = xyf_open,
};

static struct miscdevice xyf_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "xyf",
	.fops = &xyf_fops
};

static int __init xyf_init(void)
{
    int ret = misc_register(&xyf_miscdev);;
    
    return ret;
}
device_initcall(xyf_init);

MODULE_LICENSE("GPL v2");
