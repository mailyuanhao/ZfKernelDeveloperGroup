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
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("gutao");
MODULE_DESCRIPTION("simple cdev linux driver called globalmem");

#define GLOBALMEM_SIZE 1024
#define GLOBALMEM_MAJOR 200
#define GLOBALMEM_MAGIC 'g'
#define GLOBALMEM_CLEAR _IO(GLOBALMEM_MAGIC, 0)

struct globalmem_dev
{
    struct cdev cdev;
    unsigned char mem[GLOBALMEM_SIZE];
    struct mutex mutex;
};

static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);

static struct globalmem_dev *globalmem_devp;

static int globalmem_open(struct inode *inode, struct file *filp)
{
	filp->private_data = globalmem_devp;
	return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t count,
                              loff_t *f_pos)
{
    loff_t p = *f_pos;
    struct globalmem_dev *devp = (struct globalmem_dev *)(filp->private_data);
    int ret = 0;

    if (devp == NULL)
    {
        return -EINVAL;
    }

    if (p >= GLOBALMEM_SIZE) 
        return 0;
    if (count > GLOBALMEM_SIZE - p) 
        count = GLOBALMEM_SIZE - p;
    
    mutex_lock(&devp->mutex);
    if (copy_to_user(buf, devp->mem + p, count))
    {
        ret = -EFAULT;
    }
    else
    {
        *f_pos += count;
        ret = count;
    }
    mutex_unlock(&devp->mutex);

    return count;
}

static ssize_t globalmem_write(struct file *filp, const char __user *buf, size_t count,
                               loff_t *f_pos)
{
    loff_t p = *f_pos;
    unsigned long i = 0;
    int ret = 0;
    struct globalmem_dev *devp = filp->private_data;

    if (devp == NULL)
    {
        return -EINVAL;
    }

    if (p >= GLOBALMEM_SIZE)
        return 0;
    if (count > GLOBALMEM_SIZE - p) 
        count = GLOBALMEM_SIZE - p;
    
    mutex_lock(&devp->mutex);
    if (copy_from_user((void*)(devp->mem + p), buf, count))
    {
        ret = -EFAULT;
    }
    else
    {
        for(i=p; i<count; ++i)
        {
            devp->mem[i] = devp->mem[i] ^ 0x55;
        }
        *f_pos += count;
        ret = count;
    }
    mutex_unlock(&devp->mutex);

    return count;
}

loff_t globalmem_llseek(struct file *filp, loff_t offset, int whence)
{
	loff_t ret = 0;

	switch (whence)
    {
        case 0: //SEEK_SET
            if (offset < 0)
            {
                ret = -EINVAL;
                break;
            }
            if ((unsigned int)offset > GLOBALMEM_SIZE)
            {
                ret = -EINVAL;
                break;
            }
            filp->f_pos = (unsigned int)offset;
            ret = filp->f_pos;
            break;
        case 1: //SEEK_CUR
            if ((filp->f_pos + offset) > GLOBALMEM_SIZE)
            {
                ret = -EINVAL;
                break;
            }
            if ((filp->f_pos + offset) < 0)
            {
                ret = -EINVAL;
                break;
            }
            filp->f_pos += (unsigned int)offset;
            ret = filp->f_pos;
            break;
        default:
            ret = -EINVAL;
            break;
    }
	
	return ret;
}

static long globalmem_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
    struct globalmem_dev *devp = filp->private_data;
    if (devp == NULL)
    {
        return -EINVAL;
    }
    
    switch (cmd)
    {
    case GLOBALMEM_CLEAR:
        mutex_lock(&devp->mutex);
        memset(devp->mem, 0, GLOBALMEM_SIZE);
        mutex_unlock(&devp->mutex);
        break;
    default:
        return -EINVAL;
    }
    
    return 0;
}

static struct file_operations globalmem_fops = 
{
	.owner =    THIS_MODULE,
	.llseek =   globalmem_llseek,
	.read =     globalmem_read,
	.write =    globalmem_write,
	.open =     globalmem_open,
    .compat_ioctl = globalmem_ioctl,
    .unlocked_ioctl = globalmem_ioctl,
	.release =  globalmem_release,
};

static void globalmem_setup_cdev(struct globalmem_dev *dev, int index)
{
	int err, devno = MKDEV(globalmem_major, index);

	cdev_init(&dev->cdev, &globalmem_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &globalmem_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk("Error %d adding globalmem%d", err, index);
}

static int __init globalmem_init_module(void)
{
    int ret = 0;
    dev_t devno = 0;

    if (globalmem_major)
    {
        devno = MKDEV(globalmem_major, 0);
		ret = register_chrdev_region(devno, 1, "globalmem");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, 1, "globalmem");
        globalmem_major = MAJOR(devno);
    }
    if (ret < 0)
    {
		printk("globalmem: can't get dev major num:%d\n", globalmem_major);
		return ret;
	}
    
    globalmem_devp = kzalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
	if (!globalmem_devp)
    {
		ret = -ENOMEM;
		goto fail_malloc;
	}
    
    mutex_init(&globalmem_devp->mutex);
    globalmem_setup_cdev(globalmem_devp, 0);
    return 0;

fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
}

static void globalmem_cleanup_module(void)
{
    cdev_del(&globalmem_devp->cdev);
    kfree(globalmem_devp);
    unregister_chrdev_region(MKDEV(globalmem_major, 0), 1);
}

module_init(globalmem_init_module);
module_exit(globalmem_cleanup_module);
