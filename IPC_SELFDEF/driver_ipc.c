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

//trasaction between client and server
struct xyf_transaction {
    int pid;
    int data;
};

#define SET_AS_SERVER _IOW('x', 7, __s32)
#define SERVER_ENTER_LOOP _IOWR('x', 8, struct xyf_transaction)
#define SERVER_REPLY _IOWR('x', 9, struct xyf_transaction)
#define CLIENT_REQUEST _IOWR('x', 10, struct xyf_transaction)

//notify and save data
struct xyf_device
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

static void show(struct xyf_device *device)
{
    printk("pid=%d handle=%d.\n", device->pid, device->handle);
}

static struct xyf_device * find_server(void){
    struct xyf_device *item;
    hlist_for_each_entry(item, &xyf_list, list_node){
        if(item->handle == 0){
            return item;
        }
    }
    return NULL;
}

static struct xyf_device * find_client(int handle){
    struct xyf_device *item;
    hlist_for_each_entry(item, &xyf_list, list_node){
        if(item->handle == handle){
            return item;
        }
    }
    return NULL;
}

static long xyf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    struct xyf_device *server;
    struct xyf_device *client;
    struct xyf_device *device = filp->private_data;
    void __user *ubuf = (void __user *)arg;
    struct xyf_transaction xyf_data;
    
    printk("xyf_ioctl cmd: %d\n", cmd);

    switch (cmd) {
        case SERVER_ENTER_LOOP:{//server
            struct xyf_transaction __user *xyf = ubuf;
            device->data = 0;
            printk("xyf_ioctl server enter loop...\n");
            if (wait_event_interruptible(device->wait, (device->data != 0))){
                return 0;
            }
            printk("xyf_ioctl server get data from client %d\n", device->data);
            if (put_user(device->data, &xyf->data)) {
                return -EINVAL;
            }
            if (put_user(device->target_pid, &xyf->pid)) {
                return -EINVAL;
            }
            return 0;
        }
        case CLIENT_REQUEST:{//client request
            struct xyf_transaction __user *xyf_return = ubuf;
            device->data = 0;
            //target is server,there is onley one server
            device->target = 0;
            if (copy_from_user(&xyf_data, ubuf, sizeof(struct xyf_transaction))){
                return -EFAULT;
            }
            //server = hlist_entry(xyf_list.first->next, struct xyf_device, list_node);
            server = find_server();

            if(server == NULL){
                printk("xyf_ioctl cannot find server.\n");
                return 0;
            }
            device->target_pid = server->pid;
            
            server->data = xyf_data.data;
            server->target = device->handle;
            server->target_pid = device->pid;
            wake_up_interruptible(&server->wait);
            printk("xyf_ioctl client wake up server: %d,pid=%d,data=%d.\n",
                   server->handle,server->pid,server->data);
            
            //wait for server response
            printk("xyf_ioctl client waiting on %d.\n", device->handle);
            if (wait_event_interruptible(device->wait, (device->data != 0))){
                return 0;
            }
            
            printk("xyf_ioctl client response from server %d.\n", device->data);
            if (put_user(device->data, &xyf_return->data)) {
                return -EINVAL;
            }
            if (put_user(device->target_pid, &xyf_return->pid)) {
                return -EINVAL;
            }
            return 0;
        }
        case SERVER_REPLY:{//server response
            printk("xyf_ioctl server response.\n");
            if (copy_from_user(&xyf_data, ubuf, sizeof(struct xyf_transaction))){
                return -EFAULT;
            }

            client = find_client(device->target);
            if(client == NULL){
                printk("xyf_ioctl cannot find client.\n");
                return 0;
            }
            client->data = xyf_data.data;
            printk("xyf_ioctl wake up client %d.\n", client->handle);
            wake_up_interruptible(&client->wait);
            return 0;
        }
        case SET_AS_SERVER:{
            device->handle = 0;
            device->data = 0;
            printk("xyf_ioctl set as server %d.\n", device->handle);
            break;
        }
        default:
            break;
    }
    return 0;
}

static int generate_handle(void){
    int handle;
    int count;
    struct xyf_device *item;
    
    handle = 0;
    count = 0;
    
    hlist_for_each_entry(item, &xyf_list, list_node){
        count++;
    }
    if(count == 0){
        handle = 0;//server handle
    }else{
        handle = count;
    }
    return handle;
}

static int xyf_open(struct inode *nodp, struct file *filp)
{
    struct xyf_device *device;
    struct xyf_device *item;
    int handle;
    device = (struct xyf_device *)kzalloc(sizeof(*device), GFP_KERNEL);
    if (device == NULL){
        printk("xyf_open kzalloc failed.\n");
        return -ENOMEM;
    }
    init_waitqueue_head(&device->wait);
    if(hlist_empty(&xyf_list)){
        device->handle = 0;
    }else{
        handle = generate_handle();
        printk("xyf_open generate handle %d.\n", handle);
        device->handle = handle;
    }
    device->target = -1;
    device->target_pid = 0;
    hlist_add_head(&device->list_node, &xyf_list);
    device->data = 0;
    device->pid = current->pid;
    filp->private_data = device;
    printk("xyf_open finished %d.\n", device->pid);

    hlist_for_each_entry(item, &xyf_list, list_node){
        show(item);
    }
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
