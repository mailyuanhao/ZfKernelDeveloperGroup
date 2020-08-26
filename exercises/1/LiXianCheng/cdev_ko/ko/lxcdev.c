#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/kfifo.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lxc");
MODULE_DESCRIPTION("this is a first char device driver");

#define BUFF_LEN 4096 //FIFO 以及临时缓冲区大小

// ioctl相关 
#define LXC_IOC_MAGIC 'L' // 魔术字
#define LXC_IOCTL_GET_FIFO_LEN _IOR(LXC_IOC_MAGIC,1, unsigned long)

// 自定义数据结构，存储设备信息等
struct dev_data
{
	unsigned char dev_buff[BUFF_LEN]; // 存储临时加密的数据
	struct cdev dev_cdev; // 设备信息
	struct kfifo dev_fifo; // 存储加密后数据 
	struct semaphore dev_sem; // 同步信号量
	dev_t dev_id; // 设备id	
};

// 全局设备信息
struct dev_data * global_data = NULL;

// 获取当前FIFO中存储数据长度
long get_fifo_len(struct file *filp, unsigned long arg)
{
	long result = 0;
	unsigned int fifo_len = 0;

	printk(KERN_ALERT"lxc:user ptr = %p\n", (void *)arg);

	if (1 != access_ok(VERIFY_WRITE, arg, sizeof(unsigned long)))
	{
		printk(KERN_ERR"lxc:invalid user ptr\n");
		return -EFAULT;
	}

	if (0 != down_interruptible(&global_data->dev_sem))
	{
		printk(KERN_ERR"lxc:wait sem error\n");
		return -ERESTARTSYS;
	}

	fifo_len = kfifo_len(&global_data->dev_fifo);
	if (0 != copy_to_user((void __user *)arg, &fifo_len, sizeof(unsigned long)))
	{
		printk(KERN_ERR"lxc:copy_to_user error\n");
		result = -EFAULT;
	}

	up(&global_data->dev_sem);

	return result;
}

// ioctl实现
long lxc_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long result = 0;
	printk(KERN_DEBUG"lxc:lxc_unlocked_ioctl, cmd = %u\n", cmd);
	switch (cmd)
	{
		case LXC_IOCTL_GET_FIFO_LEN:	
			result = get_fifo_len(filp, arg);
			break;
		default:
			result = -ENOTTY;
			break;
	}

	return 0;
}

// ioctl实现，当前未使用到
long lxc_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long result = 0;
	printk(KERN_DEBUG"lxc:lxc_compat_ioctl, cmd = %u\n", cmd);
	switch (cmd)
	{
		case LXC_IOCTL_GET_FIFO_LEN:	
			result = get_fifo_len(filp, arg);
			break;
		default:
			result = -ENOTTY;
			break;
	}
	return 0;
}

int lxc_open(struct inode *inodp, struct file *filp)
{
	printk(KERN_DEBUG"lxc:lxc_open\n");
	return 0;
}

ssize_t lxc_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	ssize_t result = 0;
	size_t read_len = 0;
	unsigned int fifo_len = 0;

	printk(KERN_DEBUG"lxc:lxc_read\n");

	if (0 == count)
	{
		printk(KERN_DEBUG"lxc:do nothing when count is 0\n");
		return 0;
	}

	if (0 != down_interruptible(&global_data->dev_sem))
	{
		printk(KERN_ERR"lxc:wait sem error\n");
		return -ERESTARTSYS;
	}

	do
	{
		if (kfifo_is_empty(&global_data->dev_fifo))
		{
			printk(KERN_ERR"lxc:fifo is empty now\n");
			result = 0;			
		}
		else
		{
			fifo_len = kfifo_len(&global_data->dev_fifo);
			printk(KERN_DEBUG"lxc:fifo now len = %d\n", fifo_len);

			read_len = (fifo_len >= count) ? count : fifo_len;

			memset(global_data->dev_buff, 0, BUFF_LEN);
			result = kfifo_out(&global_data->dev_fifo, global_data->dev_buff, read_len);

			if (0 != copy_to_user(buff, global_data->dev_buff, result))
			{
				printk(KERN_ERR"lxc,copy_to_user error\n");
				result = -EFAULT;
				break;
			}
			printk(KERN_DEBUG"lxc,success out len %d\n", result);
		}
	}
	while (false);

	up(&global_data->dev_sem);

	return result;
}

ssize_t lxc_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp)
{
	size_t remain_len = 0;
	size_t writen_len = 0;
	size_t index = 0;
	char * cc = NULL;
	ssize_t result = 0;
	unsigned int fifo_len = 0;

	printk(KERN_DEBUG"lxc:lxc_write\n");
		
	if (0 == count)
	{
		printk(KERN_DEBUG"lxc:do nothing when count is 0\n");
		return 0;
	}

	if (0 != down_interruptible(&global_data->dev_sem))
	{
		printk(KERN_ERR"lxc:wait sem error\n");
		return -ERESTARTSYS;
	}

	do
	{
		// 根据输入当前的长度，计算剩余可以写入的长度
		if (kfifo_is_full(&global_data->dev_fifo))
		{
			printk(KERN_DEBUG"lxc:fifo is full\n");
			result = 0;
			break;
		}

		fifo_len = kfifo_len(&global_data->dev_fifo);

		if (fifo_len < BUFF_LEN)
		{
			remain_len = BUFF_LEN - fifo_len;
			writen_len = (count >= remain_len) ? remain_len : count;
				
			// copy data from user address
			memset(global_data->dev_buff, 0, BUFF_LEN);
			if (0 != copy_from_user(global_data->dev_buff, buff, writen_len))
			{
				printk(KERN_ERR"lxc:copy_from_user error\n");
				result = -EFAULT;
			}
			else
			{
				// 对输入的数据，逐个进行加密
				for (index = 0; index < writen_len; ++ index)
				{
					cc = global_data->dev_buff + index;
					*cc ^= 0x55;
				}

				result = kfifo_in(&global_data->dev_fifo, global_data->dev_buff, writen_len);
				printk(KERN_DEBUG"lxc:push fifo len = %d\n", result);
			}
		}
		else
		{
			printk(KERN_DEBUG"lxc:fifo is full\n");
			result = 0;	
		}
	}
	while (false);
	
	up(&global_data->dev_sem);

	return result;
}

int lxc_release(struct inode *inodp, struct file *filp)
{
	printk(KERN_DEBUG"lxc:lxc_release\n");
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
	.compat_ioctl = lxc_compat_ioctl,
};

struct class * lxcdev_class = NULL;

static int __init lxcdev_init(void)
{
	int result = 0;
	struct device * dev_instance = NULL;

	printk(KERN_DEBUG"lxc:dev_init\n");

	do
	{
		// 分配全局内存
		global_data = (struct dev_data *) kmalloc(sizeof(struct dev_data), GFP_KERNEL);
		if (NULL == global_data)
		{
			printk(KERN_ERR"lxc:init, kmalloc error\n");
			result = -ENOMEM;
			goto exit;
		}
		memset(global_data, 0, sizeof(struct dev_data));
		
		// 分配FIFO
		result = kfifo_alloc(&global_data->dev_fifo, BUFF_LEN, GFP_KERNEL);
		if (0 != result)
		{
			printk(KERN_ERR"lxc:init, kfifo_init error:%d\n", result);
			result = -ENOMEM;
			goto release_global; 
		}

		// 初始化信号量
		sema_init(&global_data->dev_sem, 1);

		// 分配设备号
		result = alloc_chrdev_region(&(global_data->dev_id), 0, 1, "lxcdev");	
		if (0 != result)
		{
			printk(KERN_ERR"lxc:init, alloc_chardev_region error:%d\n", result);	
			goto release_fifo;
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

		goto exit;
	}
	while (false);

destroy_class:
	class_destroy(lxcdev_class);

del_cdev:
	cdev_del(&global_data->dev_cdev);

unregister_cdev:
	unregister_chrdev_region(global_data->dev_id, 1);	

release_fifo:
	kfifo_free(&global_data->dev_fifo);	

release_global:
	kfree(global_data);

exit:
	printk(KERN_ALERT"lxc:init, return result %d\n", result);
	return result;
}

static void __exit lxcdev_uninit(void)
{
	printk(KERN_DEBUG"lxc:dev_uninit\n");

	device_destroy(lxcdev_class, global_data->dev_id);
	class_destroy(lxcdev_class);
	cdev_del(&global_data->dev_cdev);
	unregister_chrdev_region(global_data->dev_id, 1);
	kfifo_free(&global_data->dev_fifo);
	kfree(global_data);
}

module_init(lxcdev_init);
module_exit(lxcdev_uninit);

