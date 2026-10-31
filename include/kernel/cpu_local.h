#ifndef CPU_LOCAL_H
#define CPU_LOCAL_H

#include <stdint.h>

#include "kernel/spinlock.h"

#define MAX_CPUS 32

struct task; // Forward declaration

struct cpu_local {
    struct cpu_local *self; /* Must be at offset 0 */
    uint32_t cpu_id;
    struct task *current_task;
    struct task *idle_task;
    uint64_t scheduler_tick;
    uint64_t kernel_stack;
    
    spinlock_t rq_lock;
    int rq[32]; // Array of task_ids assigned to this CPU
    int rq_count;
};

void cpu_local_init(uint32_t cpu_id);
struct cpu_local *get_cpu(int index);
int get_cpu_count(void);

static inline struct cpu_local *cpu_current(void) {
    struct cpu_local *cpu;
    __asm__ volatile("mov %%gs:0, %0" : "=r"(cpu));
    return cpu;
}

static inline struct task *current_task(void) {
    struct cpu_local *cpu = cpu_current();
    if (!cpu) return 0;
    return cpu->current_task;
}

#endif
