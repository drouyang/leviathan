/* 
 * HIO Main Interface
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>             /* device file */
#include <linux/types.h>          /* dev_t */
#include <linux/kdev_t.h>         /* MAJOR MINOR MKDEV */
#include <linux/device.h>         /* udev */
#include <linux/cdev.h>           /* cdev_init cdev_add */
#include <linux/moduleparam.h>    /* module_param */
#include <linux/stat.h>           /* perms */
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#include "hio_ioctl.h"                /* device file ioctls*/
#include "hio.h"

int                      hio_major_num = 0;
struct class           * hio_class     = NULL;
static struct cdev       hio_cdev;  


static int 
device_open(struct inode * inode, 
	    struct file  * file) 
{
    return 0;
}


static int 
device_release(struct inode * inode,
	       struct file  * file)
{
    return 0;
}

static ssize_t 
device_read(struct file * file, 
	    char __user * buffer,
	    size_t        length, 
	    loff_t      * offset)
{
    printk(KERN_INFO "Read\n");
    return -EINVAL;
}


static ssize_t 
device_write(struct file       * file,
	     const char __user * buffer,
	     size_t              length, 
	     loff_t            * offset) 
{
    printk(KERN_INFO "Write\n");
    return -EINVAL;
}


static long 
device_ioctl(struct file  * filp,
	     unsigned int   ioctl,
	     unsigned long  arg) 
{
    int ret = 0;
    //void __user * argp = (void __user *)arg;
    struct hio_engine * hio_engine = (struct hio_engine *)filp->private_data;

    switch (ioctl) {
        // Given an ID, get an fd associated with the request queue of the ID
        case HIO_IOCTL_REGISTER:
            ret = stub_register(hio_engine, arg);
            break;

        default:
            printk(KERN_ERR "Invalid HIO IOCTL: %d\n", ioctl);
            ret = -EINVAL;
    }

    return ret;
}


static struct file_operations fops = {
    .owner            = THIS_MODULE,
    .read             = device_read,
    .write            = device_write,
    .unlocked_ioctl   = device_ioctl,
    .compat_ioctl     = device_ioctl,
    .open             = device_open,
    .release          = device_release
};


int 
hio_init(void) 
{
    dev_t dev_num   = MKDEV(0, 0); // <major , minor> 

    if (alloc_chrdev_region(&dev_num, 0, MAX_STUBS + 1, "hio") < 0) {
        printk(KERN_ERR "Error allocating hio char device region\n");
        return -1;
    }

    hio_major_num = MAJOR(dev_num);
    dev_num       = MKDEV(hio_major_num, MAX_STUBS + 1);

    //printk(KERN_INFO "<Major, Minor>: <%d, %d>\n", MAJOR(dev_num), MINOR(dev_num));

    if ((hio_class = class_create(THIS_MODULE, "hio")) == NULL) {
        printk(KERN_ERR "Error creating hio device class\n");
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    if (device_create(hio_class, NULL, dev_num, NULL, "hio") == NULL) {
        printk(KERN_ERR "Error creating hio device\n");
        class_destroy(hio_class);
        unregister_chrdev_region(dev_num, MAX_STUBS + 1);
        return -1;
    }

    cdev_init(&hio_cdev, &fops);

    if (cdev_add(&hio_cdev, dev_num, 1) == -1) {
        printk(KERN_ERR "Error adding hio cdev\n");
        device_destroy(hio_class, dev_num);
        class_destroy(hio_class);
        unregister_chrdev_region(dev_num, MAX_STUBS + 1);
        return -1;
    }

    return 0;
}

void 
hio_exit(void) 
{
    dev_t dev_num = MKDEV(hio_major_num, MAX_STUBS + 1);
    unregister_chrdev_region(MKDEV(hio_major_num, 0), MAX_STUBS + 1);
    cdev_del(&hio_cdev);
    device_destroy(hio_class, dev_num);
    class_destroy(hio_class);
}



module_init(hio_init);
module_exit(hio_exit);

MODULE_LICENSE("GPL");
