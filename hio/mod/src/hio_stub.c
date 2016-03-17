/* 
 * HIO Stub Interface
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

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


static long 
stub_ioctl(struct file  * filp,
	      unsigned int   ioctl,
	      unsigned long  arg) 
{
    void __user           * argp    = (void __user *)arg;
    struct hio_stub * stub = (struct hio_stub *)filp->private_data;
    struct hio_engine * hio_engine = stub->hio_engine;
    int ret = 0;

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


int 
stub_register(struct hio_engine *hio_engine, int app_id)
{
    struct hio_stub *stub = NULL;

    stub = kmalloc(sizeof(struct hio_stub), GFP_KERNEL);

    if (IS_ERR(stub)) {
        printk(KERN_ERR "Could not allocate stub state\n");
        return -1;
    }

    memset(stub, 0, sizeof(struct hio_stub));

    stub->app_id       = app_id;
    stub->dev          = MKDEV(hio_major_num, app_id);

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

    return 0;
}
