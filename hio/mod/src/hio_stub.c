/* 
 * HIO Stub Interface
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "hio.h"
#include "hio_ioctl.h"

extern int                      hio_major_num;
extern struct class           * hio_class;

static int 
stub_open(struct inode * inode, 
	     struct file  * filp) 
{
    struct hio_stub * stub = container_of(inode->i_cdev, struct hio_stub, cdev);
    filp->private_data = stub;
    return 0;
}


static int
stub_release(struct inode * inode, 
		struct file  * filp) 
{
    return 0;
}


static ssize_t 
stub_read(struct file  * filp, 
	     char __user  * buffer,
	     size_t         length, 
	     loff_t       * offset) 
{
    return 0;
}


static ssize_t 
stub_write(struct file        * filp,
	      const char __user  * buffer,
	      size_t               length, 
	      loff_t             * offset) 
{
    return 0;
}

static struct stub_syscall_t *
stub_syscall_poll(struct hio_stub *stub) 
{
    wait_event_interruptible(stub->syscall_wq, stub->is_pending);
    return stub->pending_syscall;
}

static int
stub_syscall_ret(struct hio_stub *stub, struct stub_syscall_ret_t *syscall_ret) 
{
    int ret = 0;
    struct hio_engine *engine = stub->hio_engine;
    struct hio_cmd_t *hio_cmd;

    if (engine->rb_ret_prod_idx == engine->rb_syscall_cons_idx) {
        printk(KERN_ERR "No available rb slot for syscall_ret\n");
        printk(KERN_ERR "Return syscall before handling???\n");
        return -1;
    }
    // add ret to io_engine
    spin_lock(&engine->lock);
    hio_cmd = &engine->rb[engine->rb_ret_prod_idx];
    hio_cmd->ret_val = syscall_ret->ret_val;
    hio_cmd->errno = syscall_ret->errno;
    engine->rb_ret_prod_idx = (engine->rb_ret_prod_idx+1) % HIO_RB_SIZE;
    spin_unlock(&engine->lock);
    
    // clear pending
    kfree(stub->pending_syscall);
    spin_lock(&stub->lock);
    stub->pending_syscall = NULL;
    stub->is_pending = false;
    spin_unlock(&stub->lock);

    return ret;
}

static long 
stub_ioctl(struct file  * filp,
	      unsigned int   ioctl,
	      unsigned long  arg) 
{
    void __user           * argp    = (void __user *)arg;
    struct hio_stub * stub = (struct hio_stub *)filp->private_data;
    //struct hio_engine * hio_engine = stub->hio_engine;
    int ret = 0;
    
    switch (ioctl) {
        // Poll a syscall request
        case HIO_STUB_SYSCALL_POLL: 
            {
                struct stub_syscall_t *syscall;

                syscall = stub_syscall_poll(stub);
                if (syscall == NULL) {
                    ret = -EFAULT;
                    break;
                }

                if (copy_to_user(argp, syscall, sizeof(struct stub_syscall_t))) {
                    printk(KERN_ERR "Could not copy syscall to user space\n");
                    ret = -EFAULT;
                    break;
                }
                break;
            }

        // Syscall request completed
        case HIO_STUB_SYSCALL_RET:
            {
                struct stub_syscall_ret_t syscall_ret;
                memset(&syscall_ret, 0, sizeof(struct stub_syscall_ret_t));
                if (copy_from_user(&syscall_ret, argp, sizeof(struct stub_syscall_ret_t))) {
                    printk(KERN_ERR "Could not copy syscall_ret from user space\n");
                    ret = -EFAULT;
                    break;
                }
                
                if (stub_syscall_ret(stub, &syscall_ret) < 0) {
                    printk(KERN_ERR "Failed to write syscall_ret to io_engine\n");
                    ret = -EFAULT;
                    break;
                }

                break;
            }

        // Delegate a syscall, this is for test purpose
        case HIO_STUB_SYSCALL:
            {
                struct stub_syscall_t syscall;
                memset(&syscall, 0, sizeof(struct stub_syscall_t));
                if (copy_from_user(&syscall, argp, sizeof(struct stub_syscall_t))) {
                    printk(KERN_ERR "Could not copy syscall from user space\n");
                    ret = -EFAULT;
                    break;
                }

                ret = hio_engine_syscall(stub->hio_engine, &syscall);

                break;
            }

        default:
            printk(KERN_ERR "Invalid Stub IOCTL: %d\n", ioctl);
            ret = -EINVAL;
    }

    return ret;
}


static struct file_operations stub_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = stub_ioctl,
    .compat_ioctl   = stub_ioctl,
    .open           = stub_open,
    .read           = stub_read, 
    .write          = stub_write,
    .release        = stub_release,
};

/* Create a hio_stub sturct and the /dev/hio-stubN file 
 * The hio_stub consists of a lock, a waitq, a pending_syscall pointer
 * and a is_pending flag
 */
int 
stub_register(struct hio_engine *hio_engine, int stub_id)
{
    struct hio_stub *stub = NULL;

    stub = kmalloc(sizeof(struct hio_stub), GFP_KERNEL);

    if (IS_ERR(stub)) {
        printk(KERN_ERR "Could not allocate stub state\n");
        return -1;
    }

    memset(stub, 0, sizeof(struct hio_stub));

    spin_lock_init(&stub->lock);
    init_waitqueue_head(&stub->syscall_wq);

    stub->stub_id       = stub_id;
    stub->dev          = MKDEV(hio_major_num, stub_id);

    cdev_init(&(stub->cdev), &stub_fops);

    stub->cdev.owner   = THIS_MODULE;
    stub->cdev.ops     = &stub_fops;
    
    if (cdev_add(&(stub->cdev), stub->dev, 1)) {
        printk(KERN_ERR "Fails to add cdev\n");
        kfree(stub);
        return -1;
    }

    if (device_create(hio_class, NULL, stub->dev, stub, "hio-stub%d", MINOR(stub->dev)) == NULL){
        printk(KERN_ERR "Fails to create device\n");
        cdev_del(&(stub->cdev));
        kfree(stub);
        return -1;
    }

    if (add_stub(hio_engine, stub_id, stub)) {
        printk(KERN_ERR "Fails to insert stub\n");
        device_destroy(hio_class, stub->dev);
        cdev_del(&(stub->cdev));
        kfree(stub);
        return -1;
    }

    return 0;
}

int 
stub_deregister(struct hio_engine *hio_engine, int stub_id)
{
    struct hio_stub * stub = lookup_stub(hio_engine, stub_id);
    if (stub != NULL) {
        remove_stub(hio_engine, stub_id);
    } else {
        printk(KERN_WARNING "Trying to deregister a non-exisiting stub, stub_id %d\n", 
                stub_id);
    }

    device_destroy(hio_class, stub->dev);
    cdev_del(&(stub->cdev));
    kfree(stub);

    return 0;
}
