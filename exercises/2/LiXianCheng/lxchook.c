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
#include <linux/kallsyms.h>
#include "lxchook.h"

// sys_call_table地址
unsigned long *sys_call_table_address = NULL;

// sys_xxx原型
asmlinkage long (*src_sys_open)(const char __user *filename, int flag, umode_t mode);
asmlinkage long lxc_sys_open(const char __user *filename, int flag, umode_t mode);

asmlinkage long (*src_sys_close)(unsigned int fd);
asmlinkage long lxc_sys_close(unsigned int fd);

// 改为可读写
int make_readwrite(unsigned long address)
{
	unsigned int level = 0;
	pte_t *pte = lookup_address(address, &level);
	if (NULL == pte)
	{
		return -1;
	}

	if (pte->pte & ~_PAGE_RW)
	{
		pte->pte |= _PAGE_RW;
	}

	return 0;
}

// 改为只读
int make_readonly(unsigned long address)
{
	unsigned int level = 0;
	pte_t *pte = lookup_address(address, &level);
	if (NULL == pte)
	{
		return -1;
	}

	pte->pte &= ~_PAGE_RW;
	return 0;
}

int get_sys_call_table(void)
{
	// 从/proc/kallsyms或/boot/System.map中读取
	sys_call_table_address = (unsigned long *)kallsyms_lookup_name("sys_call_table");
	if (0 == sys_call_table_address)
	{
		printk(KERN_ERR"lxc:lookup sys_call_table error\n");
		return -EFAULT;
	}

	printk(KERN_DEBUG"lxc:find sys_call_table:0x%lx\n", sys_call_table_address);
	return 0;
}

int hook_init(void)
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

	if (0 != make_readwrite(sys_call_table_address))
	{
		printk(KERN_ERR"lxc:make rw error\n");
		return -EFAULT;
	}

	sys_call_table_address[__NR_open] = (unsigned long)lxc_sys_open;
	sys_call_table_address[__NR_close] = (unsigned long)lxc_sys_close;

	make_readonly(sys_call_table_address); 

	return 0;
}

int hook_uninit(void)
{
	printk(KERN_DEBUG"lxc:hook_uninit\n");

	if (0 != make_readwrite(sys_call_table_address))
	{
		printk(KERN_ERR"lxc:make rw error\n");
		return -1;
	}

	sys_call_table_address[__NR_open] = (unsigned long)src_sys_open;
	sys_call_table_address[__NR_close] = (unsigned long)src_sys_close;

	make_readonly(sys_call_table_address);
	return 0;
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
