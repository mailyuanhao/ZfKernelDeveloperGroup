#ifndef HOOK_DEMO_HOOK_CTRL_H
#define HOOK_DEMO_HOOK_CTRL_H

#define HOOK_PATH_SIZE 1024

extern int hook_ctrl_init(void *parm_call_table);

extern void hook_ctrl_enable(void);

extern void hook_ctrl_disable(void);

extern void hook_ctrl_set_path(const char *path);

extern void hook_ctrl_cleanup(void);

#endif