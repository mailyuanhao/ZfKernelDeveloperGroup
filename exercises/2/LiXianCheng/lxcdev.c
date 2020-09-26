#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h> // device_create
#include <asm/uaccess.h> // access_ok
#include <linux/kfifo.h> // kfifo
#include <linux/poll.h> // poll
#include <linux/wait.h> // wake_up
#include <linux/sched.h> // wake_up 中TASK_NORMAL
#include <linux/file.h> // fget
#include "lxchook.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lxc");
MODULE_DESCRIPTION("this is a first char device driver");

#define BUFF_LEN 4096 //临时缓冲区大小

// ioctl相关 
#define LXC_IOC_MAGIC 'L' // 魔术字
#define LXC_IOCTL_GET_FIFO_LEN _IOR(LXC_IOC_MAGIC,1, unsigned long)

// 自定义数据结构，存储设备信息等
struct dev_data
{
	struct cdev dev_cdev; // 设备信息
	struct semaphore dev_sem; // 同步信号量
	dev_t dev_id; // 设备id	
} __attribute__((packed));

// 全局设备信息
struct dev_data * global_data = NULL;
struct class * lxcdev_class = NULL;

// ioctl实现
long lxc_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long result = 0;
	printk(KERN_DEBUG"lxc:lxc_unlocked_ioctl, cmd = %u\n", cmd);
	switch (cmd)
	{
		case LXC_IOCTL_GET_FIFO_LEN:	
			break;
		default:
			result = -ENOTTY;
			break;
	}

	return result;
}

// open实现
int lxc_open(struct inode *inodp, struct file *filp)
{
	printk(KERN_DEBUG"lxc:lxc_open\n");
	return 0;
}

// read实现
ssize_t lxc_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	printk(KERN_DEBUG"lxc:lxc_read\n");
	return 0;
}

// write实现
ssize_t lxc_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp)
{
	printk(KERN_DEBUG"lxc:lxc_write\n");
	return 0;
}

// release实现
int lxc_release(struct inode *inodp, struct file *filp)
{
	printk(KERN_DEBUG"lxc:lxc_release\n");
	return 0;
}

// lseek实现
loff_t lxc_llseek(struct file *filp, loff_t off, int whence)
{
	printk(KERN_DEBUG"lxc:lxc_llseek\n");
	return 0;
}

// poll实现
unsigned int lxc_poll(struct file *filp, poll_table *wait)
{
	printk(KERN_DEBUG"lxc:lxc_poll\n");
	return 0;
}

// 设备文件操作
const struct file_operations lxc_file_operations = 
{
	.owner = THIS_MODULE,
	.open = lxc_open,
	.read = lxc_read,
	.write = lxc_write,
	.release = lxc_release,
	.unlocked_ioctl = lxc_unlocked_ioctl,
	.poll = lxc_poll,
	.llseek = lxc_llseek,
};

static int __init dev_init(void)
{
	int result = 0;
	struct device * dev_instance = NULL;

	do
	{
		// 分配全局内存
		global_data = (struct dev_data *) kmalloc(sizeof(struct dev_data), GFP_KERNEL);
		if (NULL == global_data)
		{
			printk(KERN_ERR"lxc:init, kmalloc error\n");
			result = -ENOMEM;
			goto final_exit;
		}

		global_data->dev_id = 0;

		// 初始化信号量
		sema_init(&global_data->dev_sem, 1);

		// 分配设备号
		result = alloc_chrdev_region(&(global_data->dev_id), 0, 1, "lxcdev");	
		if (0 != result)
		{
			printk(KERN_ERR"lxc:init, alloc_chardev_region error:%d\n", result);	
			goto release_global;
		}

		// 初始化设备
		cdev_init(&global_data->dev_cdev, &lxc_file_operations);
		global_data->dev_cdev.owner = THIS_MODULE;

		// 添加设备
		result = cdev_add(&global_data->dev_cdev, global_data->dev_id, 1);		
		if (0 != result)
		{
			printk(KERN_ERR"lxc:init, cdev_add error:%d", result);
			goto unregister_cdev;
		}

		// 创建设备类
		lxcdev_class = class_create(THIS_MODULE, "lxcdev_class");
		if (IS_ERR(lxcdev_class))
		{
			printk(KERN_ERR"lxc:init class_create error\n");
			result = -ENOMEM;
			goto del_cdev;
		}

		// 创建一个设备，以便打开操作
		dev_instance = device_create(lxcdev_class, NULL, global_data->dev_id, 
			"lxcdev", "lxcdev%d", 0);
		if (NULL == dev_instance)
		{
			printk(KERN_ERR"lxc:init device_create error\n");
			result = -ENOMEM;
			goto destroy_class;
		}

		goto final_exit;
	}
	while (false);

destroy_class:
	class_destroy(lxcdev_class);

del_cdev:
	cdev_del(&global_data->dev_cdev);

unregister_cdev:
	unregister_chrdev_region(global_data->dev_id, 1);	

release_global:
	kfree(global_data);

final_exit:
	return result;
}

static int __init lxcdev_init(void)
{
	int result = 0;

	printk(KERN_DEBUG"lxc:dev_init\n");

	do
	{
		// 初始化设备
		result = dev_init();
		if (0 != result)
		{
			break;
		}
		
		// 初始化hook
		result = hook_init();
		if (0 != result)
		{
			break;
		}
	}
	while (false);

	printk(KERN_ALERT"lxc:init, return result %d\n", result);
	return result;
}

static void __exit lxcdev_uninit(void)
{
	printk(KERN_DEBUG"lxc:dev_uninit\n");

	hook_uninit();

	device_destroy(lxcdev_class, global_data->dev_id);
	class_destroy(lxcdev_class);
	cdev_del(&global_data->dev_cdev);
	unregister_chrdev_region(global_data->dev_id, 1);
	kfree(global_data);
}

module_init(lxcdev_init);
module_exit(lxcdev_uninit);
