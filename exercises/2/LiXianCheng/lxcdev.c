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
#include <linux/kallsyms.h> // kallsyms_lookup_name

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lxc");
MODULE_DESCRIPTION("this is a first char device driver");

unsigned int src_cr0 = 0; // 原始cr0值
unsigned long *sys_call_table_address = NULL; // 获取的sys_call_table地址

// 系统调用 sys_xxx原型
asmlinkage long (*src_sys_close)(unsigned int fd);
asmlinkage long lxc_sys_close(unsigned int fd);
asmlinkage long (*src_sys_open)(const char __user *filename, int flag, umode_t mode);
asmlinkage long lxc_sys_open(const char __user *filename, int flag, umode_t mode);

// 获取sys_call_table
static int __init get_sys_call_table(void)
{
	// 或者从/proc/kallsyms或/boot/System.map中读取
	unsigned long sys_call_table = kallsyms_lookup_name("sys_call_table");
	if (0 == sys_call_table)
	{
		printk(KERN_ERR"lxc:lookup sys_call_table address error\n");
		return -1;
	}
	else
	{
		printk(KERN_DEBUG"lxc:sys_call_table:0x%x", sys_call_table);
	}

	sys_call_table_address = (unsigned long *) sys_call_table;
	printk(KERN_DEBUG"lxc:find sys_call_table address\n");

	return 0;
}

// 关闭写保护
unsigned int close_cr(void)
{
	unsigned int cr0 = 0;
	unsigned int ret = 0;

#if defined(__i386__)
	printk(KERN_DEBUG"lxc:in x86 mode");
	asm volatile("movl %%cr0, %%eax" : "=a"(cr0));
	ret = cr0;
	cr0 &= 0xfffeffff;
	asm volatile("movl %%eax, %%cr0" : : "a"(cr0));
	return ret;
#elif defined(__x86_64__)
	printk(KERN_DEBUG"lxc:in x64 mode");
	asm volatile("movq %%cr0, %%rax" : "=a"(cr0)); //64bit
	ret = cr0;
	cr0 &= 0xfffeffff;
	asm volatile("movq %%rax, %%cr0" : : "a"(cr0)); //64bit
	return ret;
#else
	return -1;
#endif
}

// 恢复写保护
void open_cr(unsigned int old_val)
{
#if defined(__i386__)
	asm volatile("movl %%eax, %%cr0" : : "a"(old_val)); //32bit
#elif defined(__x86_x64__)
	asm volatile("movq %%rax, %%cr0" : : "a"(old_val)); //64bit
#endif
}

// hook初始化函数
static int __init hook_init(void)
{
	printk(KERN_DEBUG"lxc:hook_init\n");

	if (0 != get_sys_call_table())
	{
		printk(KERN_ERR"lxc:get_sys_call_table error\n");
		return -EFAULT;
	}
	
	// 获取原地址保存
	src_sys_open = sys_call_table_address[__NR_open];
	src_sys_close = sys_call_table_address[__NR_close];

	src_cr0 = close_cr();
	if (-1 == src_cr0)
	{
		printk(KERN_ERR"lxc:get src cr error\n");
		return -EFAULT;
	}

	// 替换系统函数地址
	sys_call_table_address[__NR_open] = (unsigned long)lxc_sys_open;
	sys_call_table_address[__NR_close] = (unsigned long)lxc_sys_close;

	open_cr(src_cr0);

	return 0;
}

static int __init lxcdev_init(void)
{
	int result = 0;

	printk(KERN_DEBUG"lxc:dev_init\n");

	do
	{
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

static void __exit hook_uninit(void)
{
	printk(KERN_DEBUG"lxc:hook_uninit\n");

	src_cr0 = close_cr();
	sys_call_table_address[__NR_open] = (unsigned long)src_sys_open;
	sys_call_table_address[__NR_close] = (unsigned long)src_sys_close;
	open_cr(src_cr0);
}

static void __exit lxcdev_uninit(void)
{
	printk(KERN_DEBUG"lxc:dev_uninit\n");
	hook_uninit();
}

asmlinkage long lxc_sys_open(const char __user *filename, int flag, umode_t mode)
{
	long result = 0;
	char *path = NULL;
	long path_len = 0;
	char *ext = NULL;
	bool can_open = true;

	path_len = strlen_user(filename);
	path = (char *)kmalloc(path_len + 1, GFP_KERNEL);

	if (NULL != path)
	{
		memset(path, 0, path_len + 1);
		if (0 == copy_from_user(path, filename, path_len))
		{
			ext = strrchr(path, '.');
			if (NULL != ext && strcasecmp(ext, ".xyz") == 0)
			{
				can_open = false;
			}
		}			

		kfree(path);
	}
	
	if (!can_open)
	{
		return -EACCES;
	}
	
	// 调用原始打开
	result = (*src_sys_open)(filename, flag, mode);
	return result;
}

asmlinkage long lxc_sys_close(unsigned int fd)
{
	long result = 0;
	result = (*src_sys_close)(fd);
	return result;
}

module_init(lxcdev_init);
module_exit(lxcdev_uninit);

