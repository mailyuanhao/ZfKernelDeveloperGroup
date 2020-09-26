#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>	/* copy_*_user */
#include <linux/device.h> /* class_create */
#include "hook_ctrl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gutao");
MODULE_DESCRIPTION("hook demo module");

#define HOOKDEMO_MAJOR 0
#define HOOKDEMO_MAGIC 'H'
#define HOOKDEMO_ENABLE_HOOK _IO(HOOKDEMO_MAGIC, 0)
#define HOOKDEMO_DISABLE_HOOK _IO(HOOKDEMO_MAGIC, 1)
#define HOOKDEMO_SET_HOOK_PATH _IOC(_IOC_WRITE, HOOKDEMO_MAGIC, 2, 1024)
    
struct hookdemo_dev
{
    struct cdev cdev;
    struct class *class;
    struct device *device;
};

static int hookdemo_major = HOOKDEMO_MAJOR;
module_param(hookdemo_major, int, S_IRUGO);

static unsigned long g_boot_sys_call_table = 0;
module_param(g_boot_sys_call_table, ulong, 0400);

static struct hookdemo_dev *hookdemo_devp;

static int hookdemo_open(struct inode *inode, struct file *filp)
{
	filp->private_data = hookdemo_devp;
	return 0;
}

static int hookdemo_release(struct inode *inode, struct file *filp)
{
	return 0;
}

loff_t hookdemo_llseek(struct file *filp, loff_t offset, int whence)
{
	return -EINVAL;
}

static long hookdemo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = -1;
    struct hookdemo_dev *devp = filp->private_data;
    char *tmp_buffer = NULL;
    if (devp == NULL)
    {
        return -EINVAL;
    }
    
    switch (cmd)
    {
    case HOOKDEMO_ENABLE_HOOK:
        hook_ctrl_enable();
        ret = 0;
        break;
    case HOOKDEMO_DISABLE_HOOK:
        hook_ctrl_disable();
        ret = 0;
        break;
    case HOOKDEMO_SET_HOOK_PATH:
        tmp_buffer = (char *)kmalloc(HOOK_PATH_SIZE, GFP_KERNEL);
        if (tmp_buffer != NULL)
        {
            if (copy_from_user(tmp_buffer, (void __user *)arg, HOOK_PATH_SIZE) == 0)
            {
                hook_ctrl_set_path(tmp_buffer);
                ret = 0;
            }
            kfree(tmp_buffer);
        }
        break;
    default:
        return -EINVAL;
    }
    
    return ret;
}

static struct file_operations hookdemo_fops = 
{
	.owner =    THIS_MODULE,
	.llseek =   hookdemo_llseek,
	.open =     hookdemo_open,
    .compat_ioctl = hookdemo_ioctl,
    .unlocked_ioctl = hookdemo_ioctl,
	.release =  hookdemo_release,
};

static int __init hookdemo_init_module(void)
{
    int ret = 0;
    dev_t devno = 0;

    if (hookdemo_major)
    {
        devno = MKDEV(hookdemo_major, 0);
		ret = register_chrdev_region(devno, 1, "hookdemo");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, 1, "hookdemo");
        hookdemo_major = MAJOR(devno);
    }
    if (ret < 0)
    {
		printk("hookdemo: can't get dev major num:%d\n", hookdemo_major);
		return ret;
	}
    
    hookdemo_devp = kzalloc(sizeof(struct hookdemo_dev), GFP_KERNEL);
	if (!hookdemo_devp)
    {
		ret = -ENOMEM;
		goto fail_kzalloc;
	}
    
    if (hook_ctrl_init((void *)g_boot_sys_call_table) != 0)
    {
        goto fail_initctrl;
    }

	cdev_init(&hookdemo_devp->cdev, &hookdemo_fops);
	hookdemo_devp->cdev.owner = THIS_MODULE;
	hookdemo_devp->cdev.ops = &hookdemo_fops;
	ret = cdev_add(&hookdemo_devp->cdev, devno, 1);
	if (ret)
    {
		printk("Error %d adding hookdemo", ret);
        goto fail_addcdev;
    }
    
    hookdemo_devp->class = class_create(THIS_MODULE, "hookdemo_class");
	if (IS_ERR(hookdemo_devp->class))
    {
		ret = PTR_ERR(hookdemo_devp->class);
        goto fail_createclass;
    }
    
    hookdemo_devp->device = device_create(hookdemo_devp->class, NULL, devno, "hookdemodev", "hookdemodev%d", 0);
    if (IS_ERR(hookdemo_devp->device))
    {
		ret = PTR_ERR(hookdemo_devp->device);
        goto fail_createdevice;
    }
    return 0;

fail_createdevice:
    class_destroy(hookdemo_devp->class);
fail_createclass:
    cdev_del(&hookdemo_devp->cdev);
fail_addcdev:
    kfree(hookdemo_devp);
fail_initctrl:
fail_kzalloc:
	unregister_chrdev_region(devno, 1);
    return ret;
}

static void hookdemo_cleanup_module(void)
{
    hook_ctrl_cleanup();
    device_destroy(hookdemo_devp->class, MKDEV(hookdemo_major, 0));
    class_destroy(hookdemo_devp->class);
    cdev_del(&hookdemo_devp->cdev);
    kfree(hookdemo_devp);
    unregister_chrdev_region(MKDEV(hookdemo_major, 0), 1);
}

module_init(hookdemo_init_module);
module_exit(hookdemo_cleanup_module);
