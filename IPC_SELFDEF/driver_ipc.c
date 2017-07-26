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

static HLIST_HEAD(xyf_list);//struct hlist_head

//trasaction between client and server
struct xyf_write_read{
    char name[10];
    size_t len;
    int out_handle;//from kernel to user space
    int in_handle; //from user space to kernel
    int data;
    int pid;
};
//present a service
struct binder_node
{
    struct binder_node *next;
    int handle;
    size_t len;
    char name[10];
};

//present a ipc transaction from client to server
struct binder_transaction{
    struct binder_thread *from;
    struct binder_proc *to_proc;
    struct binder_thread* to_thread;
    int data;
    int to_handle;
};

//present thread wait for,client or server
struct binder_thread{
    struct binder_thread *next;
    int pid;
    wait_queue_head_t wait;
    struct binder_transaction *transaction;
};

//present a process
struct binder_proc
{
    int pid;
    struct hlist_node list_node;
    struct binder_node *nodes;
    struct binder_thread *threads;
};

struct binder_transaction *create_transaction(void){
    struct binder_transaction *transaction;
    transaction = (struct binder_transaction *)kzalloc(sizeof(*transaction),GFP_KERNEL);
    return transaction;
}

//node begin
int register_service(struct binder_proc *proc,char *name, 
                     int handle,size_t len){
    struct binder_node *node;
    
    node = (struct binder_node *)kzalloc(sizeof(*node),GFP_KERNEL);
    node->handle = handle;
    node->len = len;
    memcpy(node->name, name, 10*sizeof(char));
    node->next = proc->nodes;
    proc->nodes = node;
    return 0;
}
static int handle_index = 0;
static int generate_handle(void){
    /*
    struct binder_proc *item;
    struct binder_node *node;
    int count = 0;
    
    if(hlist_empty(&xyf_list)){
        return 0;
    }

    hlist_for_each_entry(item, &xyf_list, list_node){
        for(node = item->nodes; node; node = node->next){
            count++;
        }
    }

    if(count >= 1){
        return count - 1;
    }
    return count;*/
    int count = handle_index;
    handle_index++;
    return count;
    
}
//node end

#define REGISTER_SERVICE _IOWR('x', 11, struct xyf_write_read)
#define GET_SERVICE _IOWR('x', 12, struct xyf_write_read)
#define SERVER_ENTER_LOOP _IOWR('x', 13, struct xyf_write_read)
#define SERVER_REPLY _IOWR('x', 14, struct xyf_write_read)
#define CLIENT_REQUEST _IOWR('x', 15, struct xyf_write_read)

//thread begin
struct binder_thread *binder_get_thread(struct binder_proc *proc){
    struct binder_thread *thread = NULL;
    for(thread = proc->threads; thread; thread = thread->next){
        if(current->pid == thread->pid){
            return thread;
        }
    }
    printk("xyf_ioctl cannot find thread create it.\n");
    thread = (struct binder_thread *)kzalloc(sizeof(*thread),GFP_KERNEL);
    if(thread == NULL){
        printk("xyf_ioctl thread kzalloc failed.\n");
        return NULL;
    }

    thread->pid = current->pid;
    thread->next = proc->threads;
    thread->transaction = NULL;
    proc->threads = thread;
    init_waitqueue_head(&thread->wait);

    return thread;
}

struct binder_thread *pickup_thread(struct binder_proc *proc){
    struct binder_thread *thread;
    for(thread = proc->threads; thread; thread = thread->next){
        //get the first thread
        if(thread->pid != proc->pid){
            return thread;
        }
    }
    return NULL;
}

//thread end

static struct binder_proc *find_proc(int handle){
    struct binder_proc *item;
    struct binder_node *node;
    
    hlist_for_each_entry(item, &xyf_list, list_node){
        for(node = item->nodes; node; node = node->next){
            if(node->handle == handle){
                return item;
            }
        }
    }
    return NULL;
}

static int notify_server(struct binder_thread *source, struct xyf_write_read *bwr){
    struct binder_thread *to_thread = NULL;
    struct binder_transaction *transaction = NULL;
    struct binder_thread *from = NULL;
    struct binder_proc *to_proc = find_proc(bwr->in_handle);
    
    if(to_proc == NULL){
        printk("xyf_ioctl cannot find service %d.\n",bwr->in_handle);
        return -1;
    }
    
    to_thread = pickup_thread(to_proc);
    transaction = create_transaction();
    from = source;
    
    source->transaction = transaction;

    to_thread->transaction = transaction;
    transaction->from = from;
    transaction->to_proc = to_proc;
    transaction->to_thread = to_thread;
    transaction->data = bwr->data;
    transaction->to_handle = bwr->in_handle;
    printk("xyf_ioctl notify_server %ld:%d.\n",(long)transaction, transaction->data);
    wake_up_interruptible(&to_thread->wait);
    return 0;
}

static int notify_client(struct binder_thread *source,struct xyf_write_read *bwr){
    struct binder_thread *from = NULL;
    struct binder_transaction *transaction = source->transaction;
    if(transaction == NULL){
        printk("xyf_ioctl no transaction.\n");
        return -1;
    }
    
    transaction->data = bwr->data;
    from = transaction->from;

    wake_up_interruptible(&from->wait);
    return 0;
}

static int get_handle(struct xyf_write_read *bwr){
    struct binder_proc *item;
    struct binder_node *node;
    char *name = bwr->name;
    size_t len = bwr->len;
    
    hlist_for_each_entry(item, &xyf_list, list_node){
        for(node = item->nodes; node; node = node->next){
            if((len == node->len) && !memcmp(name, node->name, len*sizeof(char))){
                return node->handle;
            }
        }
    }
    printk("xyf_ioctl cannot find handle %s\n", bwr->name);
    return -1;
}

static long xyf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    int result, handle;
    struct xyf_write_read bwr;
    struct binder_thread *thread;
    void __user *ubuf = (void __user *)arg;
    struct binder_proc *proc = filp->private_data;
    
    printk("xyf_ioctl %d:%d\n", cmd, current->pid);
    thread = binder_get_thread(proc);
    if(thread == NULL){
        printk("xyf_ioctl get thread failed!\n");
    }
    switch (cmd) {
        case SERVER_ENTER_LOOP:{//server
            struct binder_transaction * transaction = NULL;
            struct xyf_write_read __user *reply = ubuf;
            
            printk("xyf_ioctl server enter loop:%d...\n",current->pid);
            if(thread == NULL){
                return 0;
            }
            thread->transaction = NULL;
            if (wait_event_interruptible(thread->wait, (thread->transaction != NULL))){
                return 0;
            }
            transaction = thread->transaction;
            if(transaction == NULL){
                printk("xyf_ioctl no transaction in server\n");
                return 0;
            }
            printk("xyf_ioctl server get data from client %ld:%d\n", (long)transaction,transaction->data);
            
            if (put_user(transaction->data, &reply->data)) {
                return -EINVAL;
            }
            //tell which service to visit in server
            if (put_user(transaction->to_handle, &reply->out_handle)) {
                return -EINVAL;
            }

            return 0;
        }
        case CLIENT_REQUEST:{//client request
            struct xyf_write_read __user *reply = ubuf;
            struct binder_transaction *transaction = NULL;
            if(thread == NULL){
                return 0;
            }
            if (copy_from_user(&bwr, ubuf, sizeof(struct xyf_write_read))){
                return -EFAULT;
            }
            
            result = notify_server(thread, &bwr);
            if(result == -1){
                return 0;
            }
            //wait for server response
            printk("xyf_ioctl client waiting... \n");
            if (wait_event_interruptible(thread->wait, 
                        (thread->transaction->data != bwr.data))){
                return 0;
            }
            transaction = thread->transaction;
            printk("xyf_ioctl client response from server %d.\n", transaction->data);

            if (put_user(transaction->data, &reply->data)) {
                return -EINVAL;
            }
            return 0;
        }
        case SERVER_REPLY:{//server response
            printk("xyf_ioctl server response.\n");
            if(thread == NULL){
                return 0;
            }
            if (copy_from_user(&bwr, ubuf, sizeof(struct xyf_write_read))){
                return -EFAULT;
            }
            
            result = notify_client(thread, &bwr);
            if(result == -1){
                return 0;
            }
            return 0;
        }
        case REGISTER_SERVICE:{
            struct xyf_write_read __user *reply = ubuf;
            if (copy_from_user(&bwr, ubuf, 
                        sizeof(struct xyf_write_read))){
                return -EFAULT;
            }
            handle = generate_handle();
            printk("xyf_ioctl generate_handle %d\n", handle);
            register_service(proc, bwr.name, handle, bwr.len);
            if (put_user(handle, &reply->out_handle)) {
                return -EINVAL;
            }
            printk("xyf_ioctl add service %s, len = %d:%d\n", 
                   bwr.name, (int)(bwr.len),current->pid);
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
    struct binder_proc *proc;

    proc = (struct binder_proc *)kzalloc(sizeof(*proc), GFP_KERNEL);
    if (proc == NULL){
        printk("xyf_open kzalloc failed.\n");
        return -ENOMEM;
    }
    hlist_add_head(&proc->list_node, &xyf_list);
    proc->pid = current->group_leader->pid;
    proc->nodes = NULL;
    proc->threads = NULL;
    filp->private_data = proc;
    printk("xyf_open %d.\n", proc->pid);
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
