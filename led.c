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
#include <linux/semaphore.h>	//using for semaphore
#include <linux/ioport.h>	//using for request_mem_region
#include <asm/uaccess.h>	//using for copy_from_user copy_to_user
#include <asm/io.h>	//using for ioremap iounmap

#define LED_BASE		0x56000010	//physical address
#define LED_CON_BASE	0x56000010	//physical address
#define LED_UP_BASE		0x56000018	//physical address
#define LED_DAT_BASE	0x56000014	//physical address

#define LED_NUM	4
#define LED_BUFFER_SIZE	4

static int led_major = 0;
static int led_minor = 0;
volatile unsigned long *gpbcon;
volatile unsigned long *gpbup;
volatile unsigned long *gpbdat;

struct led_device{
	struct cdev cdev;
	int led_index;
	struct semaphore sem;
	char buf[LED_BUFFER_SIZE];
};

struct led_device *led_devices;

static int led_open(struct inode *inode,struct file *filp){
	struct led_device *dev = container_of(inode->i_cdev,struct led_device,cdev);	
	filp->private_data = dev;
	return 0;
}
static ssize_t led_write(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos){
	struct led_device *dev = (struct led_device *)filp->private_data;
	if(*f_pos >= LED_BUFFER_SIZE){
		*f_pos = 0;
	}
	if(*f_pos + count > LED_BUFFER_SIZE){
		count = LED_BUFFER_SIZE - *f_pos;	
	}
	if(copy_from_user(dev->buf,buf,count)){
		printk(KERN_ERR "[led/led_write]Can not write device\n");
		return -EFAULT;
	}
	*f_pos += count;
	if(!strcmp(dev->buf,"on") || !strcmp(dev->buf,"off")){
		if(!strcmp(dev->buf,"on")){	//led light on
			do{
				*gpbdat |= 0x1 << (dev->led_index);
			}while(0);
		}else{	//led light off
			do{
				*gpbdat &= ~(0x1 << (dev->led_index));		
			}while(0);
		}
	}
	printk(KERN_INFO "[led/led_wite]LED buffer is %s\n",dev->buf);
	return count;
}
static ssize_t led_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos){
	struct led_device *dev = (struct led_device *)filp->private_data;
	if(*f_pos >= LED_BUFFER_SIZE){
		*f_pos = 0;
	}
	if(*f_pos + count > LED_BUFFER_SIZE){
		count = LED_BUFFER_SIZE - *f_pos;	
	}
	if(copy_to_user(dev->buf,buf,count)){
		printk(KERN_ERR "[led/led_write]Can not write device\n");
		return -EFAULT;
	}
	*f_pos += count;
	printk(KERN_INFO "[led/led_wite]LED buffer is %s\n",dev->buf);
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

static int led_setupcdev(struct led_device *dev,int num){
	int retval = 0;
	dev_t dev_num = MKDEV(led_major,num);
	cdev_init(&dev->cdev,&led_fs);
	dev->cdev.owner = THIS_MODULE;
	dev->led_index = num + 5;	//define led index
	if( (retval = cdev_add(&dev->cdev,dev_num,1)) < 0){
		printk(KERN_ERR "[led/led_setupcdev]Can not add struct cdev");
		return retval;
	}
	return retval;
}

static int __init led_init(void){
	dev_t dev_num = 0;
	int retval = 0;
	int index = 0;
	if(!request_mem_region(LED_BASE,3,"LED_BASE")){
		printk(KERN_ERR "[led/led_init]Can not request mem region to use led devices\n");
		return -ENODEV;
	}
	gpbcon = (volatile unsigned long *)ioremap_nocache(LED_CON_BASE,1);
	gpbup = (volatile unsigned long *)ioremap_nocache(LED_UP_BASE,1);
	gpbdat = (volatile unsigned long *)ioremap_nocache(LED_DAT_BASE,1);
	if(!led_major){
		retval = alloc_chrdev_region(&dev_num,led_minor,LED_NUM,"led");
		led_major = MAJOR(dev_num);
	}else{
		dev_num = MKDEV(led_major,led_minor);
		retval = register_chrdev_region(dev_num,LED_NUM,"led");
	}
	if(retval < 0){
		printk(KERN_ERR "[led/led_init]Can not register region\n");	
		return retval;
	}
	led_devices = kmalloc(LED_NUM * sizeof(struct led_device),GFP_KERNEL);
	if(!led_devices){
		printk(KERN_ERR "[led/led_init]Can not malloc\n");
		unregister_chrdev_region(dev_num,LED_NUM);
		iounmap((void *)gpbcon);
		iounmap((void *)gpbup);
		iounmap((void *)gpbdat);
		release_mem_region(LED_BASE,3);
		return -ENOMEM;
	}
	memset(led_devices,0,LED_NUM * sizeof(struct led_device));
	for(index = 0;index < LED_NUM;index++){
		sema_init(&led_devices[index].sem,1);
		if( led_setupcdev(led_devices + index,index) < 0){
			printk(KERN_ERR "[led/led_init]Can not register cdev\n");
			unregister_chrdev_region(dev_num,LED_NUM);
			kfree(led_devices);
			iounmap((void *)gpbcon);
			iounmap((void *)gpbup);
			iounmap((void *)gpbdat);
			release_mem_region(LED_BASE,3);
			return retval;
		}
	}
	/*setup GPB5 GPB6 GPB7 GPB8 as output*/
	*gpbcon &= ~(0x11 << 10 |
		        0x11 << 12 |
		        0x11 << 14 |
		        0x11 << 16
		        );
    *gpbcon |= (0x01 << 10 |
		       0x01 << 12 |
		       0x01 << 14 |
		       0x01 << 16
		        );
	/*disable GPB5 GPB6 GPB7 GPB8 pull up */
    *gpbup &= ~(0x0 << 5 |
	           0x0 << 6 |
		       0x0 << 7 |
		       0x0 << 8 
		       );
	*gpbup |= (0x1 << 5 |
	          0x1 << 6 |
	          0x1 << 7 |
	          0x1 << 8
	         );
	printk(KERN_INFO "[led/led_init]Success initialize device\n");
	return 0;	
}
static void __exit led_exit(void){
	int index = 0;
	dev_t dev_num = MKDEV(led_major,led_minor);
	for(index = 0;index < LED_NUM;index++){
		cdev_del(&led_devices[index].cdev);
	}
	kfree(led_devices);
	unregister_chrdev_region(dev_num,LED_NUM);
	iounmap((void *)gpbcon);
	iounmap((void *)gpbup);
	iounmap((void *)gpbdat);
	release_mem_region(LED_BASE,3);
	printk(KERN_INFO "[led/led_exit]Success exit from kernel\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zengweitotty");
MODULE_DESCRIPTION("TQ2440 led device driver");
