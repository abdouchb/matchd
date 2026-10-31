#include "kernel/cpu_local.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

#define IA32_GS_BASE 0xC0000101
#define IA32_KERNEL_GS_BASE 0xC0000102

static struct cpu_local *cpus[MAX_CPUS];
static int num_cpus = 0;

struct cpu_local *get_cpu(int index) {
    if (index >= 0 && index < num_cpus) {
        return cpus[index];
    }
    return NULL;
}

int get_cpu_count(void) {
    return num_cpus;
}

static spinlock_t cpu_local_lock = {0};

void cpu_local_init(uint32_t cpu_id) {
    /* Allocate per-CPU structure */
    struct cpu_local *local = kmalloc(sizeof(struct cpu_local));
    memset(local, 0, sizeof(struct cpu_local));
    
    local->self = local;
    local->cpu_id = cpu_id;
    local->rq_lock.lock = 0; // Initialize spinlock
    
    spin_lock(&cpu_local_lock);
    if (num_cpus < MAX_CPUS) {
        cpus[num_cpus++] = local;
    }
    spin_unlock(&cpu_local_lock);
    
    /* Write to GS_BASE MSR to set up the per-CPU pointer */
    wrmsr(IA32_GS_BASE, (uint64_t)local);
    /* Also write KERNEL_GS_BASE just in case we use swapgs later */
    wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)local);
}
