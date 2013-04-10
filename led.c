/*
    file Name:      led.c
	Author:         zengweitotty
	version:        V1.0
	Data:           2013/04/10
	Email:          zengweitotty@gmail.com
	Description     TQ2440 led device driver
*/
#include <linux/init.h> //use for function module_init,module_exit
#include <linux/module.h>
#include <linux/kernel.h>   //use for printk function
#include <linux/types.h>    //type for dev_t
#include <linux/fs.h>   //use for struct file and struct file_operations
#include <linux/cdev.h> //use for cdev and some related
#include <linux/slab.h> //use for kmalloc and kfree

static int led_major = 0;
static int led_minor = 0;

struct led_device{
	struct cdev cdev;
	volatile unsigned long * led;
};

static int led_open(struct inode *inode,struct file *filp){
	struct led_device *dev = container_of(inode->i_cdev,struct led_device,cdev);	
	filp->private = dev;
	return 0;
}
static ssize_t led_write(struct file *filp,char __user *buf,size_t count,loff_t *f_pos){
	return 0;
}
static ssize_t led_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos){
	return 0;
}
static int led_release(struct inode *inode,struct file *filp){
	return 0;		
}
struct file_operations led_fs = {
	.owner = THIS_MODULE,
	.open = led_open,
	.write = led_write,
	.read = led_read,
	.release = led_release,
};

static int __init led_init(void){
	return 0;	
}
static void __exit led_exit(void){
			
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zengweitotty");
MODULE_DESCRIPTION("TQ2440 led device driver");
