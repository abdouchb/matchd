/*
 * Zarqaa OS 0.1 - Scheduler Header
 * Creator: Abdelhak Chabbi
 */

#ifndef KERNEL_SCHEDULER_H
#define KERNEL_SCHEDULER_H

#include "zarqaa/types.h"

#define MAX_TASKS 32
#define TASK_STACK_SIZE 16384

enum task_priority {
    PRIO_IDLE = 0,
    PRIO_LOW,
    PRIO_NORMAL,
    PRIO_HIGH,
    PRIO_MAX
};

void scheduler_init(void);
void task_set_priority(int task_id, enum task_priority prio);
uint64_t scheduler_switch_task(uint64_t current_rsp);
/* Create a new kernel task */
int task_create(void (*entry)(void *), void *arg, const char *name);

/* Create a new user mode task (Ring 3) */
int task_create_user(void (*entry)(void *), void *arg, const char *name);

/* Create an isolated user process */
int process_create(void (*entry)(void *), const char *name);

/* Execute an ELF file in an isolated user process */
int process_exec(const char *path, const char *name);

void task_yield(void);
void task_exit(void);
bool task_kill(int task_id);
const char *scheduler_get_current_name(void);
int scheduler_get_task_count(void);
void scheduler_get_task_info(int index, int *id, char *name, int *state);
int scheduler_get_cpu_usage(void);
int scheduler_get_task_cpu_usage(int index);
int scheduler_get_core_count(void);
int scheduler_get_core_usage(int core_id);

/* File Descriptor Management API for Syscalls */
struct vfs_node;
struct file_descriptor {
    struct vfs_node *node;
    size_t offset;
    int flags;
};
int scheduler_allocate_fd(struct vfs_node *node);
struct file_descriptor *scheduler_get_fd(int fd);
void scheduler_free_fd(int fd);

#endif /* KERNEL_SCHEDULER_H */
