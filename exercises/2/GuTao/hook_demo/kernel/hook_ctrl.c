#include <linux/init.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>

#include "hook_ctrl.h"

#define SYS_CALL_TABLE_OPEN_INDEX __NR_open

typedef asmlinkage long (*HOOK_SYS_OPEN_FUNC)(const char __user *filename, int flags, int mode);

static atomic_t g_hook_enable = ATOMIC_INIT(0);
static unsigned char g_hook_file_path[HOOK_PATH_SIZE];
static spinlock_t g_hook_file_lock;

static void **g_call_table = NULL;

static HOOK_SYS_OPEN_FUNC g_hook_open_org_func = NULL;
static atomic_t g_sys_open_hooked = ATOMIC_INIT(0);

static int disable_page_protect(void *pointer)
{
    unsigned long addr = 0;
    unsigned level = 0;
    pte_t *pte = NULL;
    int rv = -1;

    if (pointer != NULL)
    {
        addr = (unsigned long)pointer;
        pte = lookup_address(addr, &level);
        if (pte != NULL)
        {
            rv = 0;

            if (!(pte->pte & _PAGE_RW))
            {
                pte->pte |= _PAGE_RW;
                rv = 1;
            }
        }
    }

    return rv;
}

static int restore_page_protect(void *pointer)
{
    unsigned long addr = 0;
    unsigned level = 0;
    pte_t *pte = NULL;
    int rv = -1;

    if (pointer != NULL)
    {
        addr = (unsigned long)pointer;
        pte = lookup_address(addr, &level);
        if (pte != NULL)
        {
            pte->pte &= (~_PAGE_RW);
            rv = 0;
        }
    }

    return rv;
}

static long sys_open_ctrl(const char __user *filename, int flags, int mode)
{
    char *buffer = NULL;
    int name_len = 0;
    int ret = 0;

    buffer = (char *)kzalloc(HOOK_PATH_SIZE, GFP_KERNEL);
    if (buffer != NULL)
    {
        name_len = strnlen_user(filename, HOOK_PATH_SIZE);
        if (name_len > 0)
        {
            if (name_len >= HOOK_PATH_SIZE)
            {
                name_len = HOOK_PATH_SIZE-1;
            }
            if (copy_from_user(buffer, filename, name_len) == 0)
            {
                spin_lock(&g_hook_file_lock);
                if (strcmp(g_hook_file_path, buffer) == 0)
                {
                    ret = -1;
                }
                spin_unlock(&g_hook_file_lock);
            }
        }
        
        kfree(buffer);
    }

    return ret;
}

asmlinkage long sys_open_new(const char __user *filename, int flags, int mode)
{
    long ret = -EACCES;

    if (g_hook_open_org_func != NULL)
    {
        if ((atomic_read(&g_hook_enable) == 0) || (sys_open_ctrl(filename, flags, mode) == 0))
        {
            ret = g_hook_open_org_func(filename, flags, mode);
        }
    }

    return ret;
}

static int hook_register_sys_open(void)
{
    int valid = 0;
    int ret = -1;

    if (g_call_table != NULL && atomic_read(&g_sys_open_hooked) == 0)
    {
        //禁用x86内存页保护
        valid = disable_page_protect(&g_call_table[SYS_CALL_TABLE_OPEN_INDEX]);
        if (valid != -1)
        {
            //保存系统调用表原地址
            g_hook_open_org_func = g_call_table[SYS_CALL_TABLE_OPEN_INDEX];
            //替换系统调用表地址到自定义地址
            g_call_table[SYS_CALL_TABLE_OPEN_INDEX] = sys_open_new;

            //恢复x86内存页保护
            restore_page_protect(&(g_call_table[SYS_CALL_TABLE_OPEN_INDEX]));
            atomic_set(&g_sys_open_hooked, 1);
            ret = 0;
        }
    }

    return ret;
}

static void hook_unregister_sys_open(void)
{
    int valid = 0;

    if (g_call_table != NULL && atomic_read(&g_sys_open_hooked) == 1)
    {
        //禁用x86内存页保护
        valid = disable_page_protect(&g_call_table[SYS_CALL_TABLE_OPEN_INDEX]);
        if (valid != -1)
        {
            //恢复系统调用表原地址
            g_call_table[SYS_CALL_TABLE_OPEN_INDEX] = g_hook_open_org_func;
            
            //恢复x86内存页保护
            restore_page_protect(&(g_call_table[SYS_CALL_TABLE_OPEN_INDEX]));
            atomic_set(&g_sys_open_hooked, 0);
        }
    }
}

int hook_ctrl_init(void *parm_call_table)
{
    spin_lock_init(&g_hook_file_lock);
    
    g_call_table = (void **)kallsyms_lookup_name("sys_call_table");
    if (g_call_table == NULL)
    {
        g_call_table = (void **)parm_call_table;
    }
    if (g_call_table == NULL)
    {
        printk("call_table is NULL\n");
        return -1;
    }
    
    return 0;
}

void hook_ctrl_enable(void)
{
    if (atomic_read(&g_hook_enable) == 0)
    {
        hook_register_sys_open();
        atomic_set(&g_hook_enable, 1);
    }
}

void hook_ctrl_disable(void)
{
    atomic_set(&g_hook_enable, 0);
}

void hook_ctrl_set_path(const char *path)
{
    spin_lock(&g_hook_file_lock);
    snprintf(g_hook_file_path, sizeof(g_hook_file_path), "%s", path);
    spin_unlock(&g_hook_file_lock);
}

void hook_ctrl_cleanup(void)
{
    hook_unregister_sys_open();
}
