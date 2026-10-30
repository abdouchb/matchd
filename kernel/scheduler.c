/*
 * Zarqaa OS 0.1 - Basic Round-Robin Scheduler
 * Creator: Abdelhak Chabbi
 */

#include "kernel/scheduler.h"
#include "kernel/heap.h"
#include "kernel/vmm.h"
#include "zarqaa/string.h"
#include "kernel/spinlock.h"
#include "kernel/cpu_local.h"
#include "filesystem/vfs.h"
enum task_state {
    TASK_DEAD,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_ZOMBIE
};

#define MAX_FDS 32

struct task {
    char name[32];
    uint64_t rsp;
    uint64_t stack_base;
    enum task_state state;
    int task_id;
    uint64_t ticks;
    uint64_t cr3;
    enum task_priority priority;
    
    struct file_descriptor *fds[MAX_FDS];
    uint64_t heap_end;
};

static struct task tasks[MAX_TASKS];
static spinlock_t scheduler_lock = SPINLOCK_INIT;
static int next_task_id = 1;
static bool scheduler_enabled = false;
static uint64_t total_ticks = 0;

void task_reaper(void) {
    extern void kfree(void*);
    extern void vmm_destroy_pml4(uint64_t cr3);
    struct cpu_local *cpu = cpu_current();
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_ZOMBIE) {
            if (tasks[i].stack_base != 0) {
                kfree((void *)tasks[i].stack_base);
                tasks[i].stack_base = 0;
            }
            if (tasks[i].cr3 != 0 && tasks[i].cr3 != tasks[0].cr3) {
                vmm_destroy_pml4(tasks[i].cr3);
                tasks[i].cr3 = 0;
            }
            tasks[i].state = TASK_DEAD;
            
            // Remove from CPU run queue
            if (cpu) {
                spin_lock(&cpu->rq_lock);
                for (int j = 0; j < cpu->rq_count; j++) {
                    if (cpu->rq[j] == i) {
                        for (int k = j; k < cpu->rq_count - 1; k++) {
                            cpu->rq[k] = cpu->rq[k + 1];
                        }
                        cpu->rq_count--;
                        break;
                    }
                }
                spin_unlock(&cpu->rq_lock);
            }
        }
    }
}

static void idle_task(void *arg) {
    (void)arg;
    while (1) {
        task_reaper();
        hlt();
    }
}

void scheduler_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_DEAD;
    }
    
    struct cpu_local *cpu = cpu_current();
    if (!cpu) panic("scheduler_init called before cpu_local_init");

    /* Register the current running thread (main kernel thread) */
    tasks[0].state = TASK_RUNNING;
    strncpy(tasks[0].name, "kernel_main", 31);
    tasks[0].task_id = next_task_id++;
    tasks[0].ticks = 0;
    tasks[0].priority = PRIO_NORMAL;
    
    uint64_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
    tasks[0].cr3 = current_cr3;
    
    cpu->current_task = &tasks[0];
    cpu->rq[cpu->rq_count++] = 0; // Add main thread to run queue
    
    // Create idle task for BSP
    int idle_id = task_create(idle_task, NULL, "idle");
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == idle_id) {
            tasks[i].priority = PRIO_IDLE;
            cpu->idle_task = &tasks[i];
            
            // Remove idle task from the regular run queue so it can't be stolen
            spin_lock(&cpu->rq_lock);
            for (int j = 0; j < cpu->rq_count; j++) {
                if (cpu->rq[j] == i) {
                    for (int k = j; k < cpu->rq_count - 1; k++) {
                        cpu->rq[k] = cpu->rq[k+1];
                    }
                    cpu->rq_count--;
                    break;
                }
            }
            spin_unlock(&cpu->rq_lock);
            break;
        }
    }
    
    scheduler_enabled = true;
}

void scheduler_ap_setup(void) {
    struct cpu_local *cpu = cpu_current();
    if (!cpu) return;

    int idle_id = task_create(idle_task, NULL, "idle_ap");
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == idle_id) {
            tasks[i].priority = PRIO_IDLE;
            cpu->idle_task = &tasks[i];
            
            // Current task is idle task initially
            cpu->current_task = cpu->idle_task;
            tasks[i].state = TASK_RUNNING;
            
            // Remove idle task from the regular run queue so it can't be stolen
            spin_lock(&cpu->rq_lock);
            for (int j = 0; j < cpu->rq_count; j++) {
                if (cpu->rq[j] == i) {
                    for (int k = j; k < cpu->rq_count - 1; k++) {
                        cpu->rq[k] = cpu->rq[k+1];
                    }
                    cpu->rq_count--;
                    break;
                }
            }
            spin_unlock(&cpu->rq_lock);
            break;
        }
    }
}

void task_set_priority(int task_id, enum task_priority prio) {
    spin_lock(&scheduler_lock);
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_DEAD && tasks[i].task_id == task_id) {
            tasks[i].priority = prio;
            break;
        }
    }
    spin_unlock(&scheduler_lock);
}

int task_create(void (*entry)(void *), void *arg, const char *name) {
    spin_lock(&scheduler_lock);
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        spin_unlock(&scheduler_lock);
        return -1; // Max tasks reached
    }

    uint8_t *stack;
    if (tasks[slot].stack_base != 0) {
        stack = (uint8_t *)tasks[slot].stack_base;
    } else {
        stack = kmalloc(TASK_STACK_SIZE);
        if (!stack) {
            spin_unlock(&scheduler_lock);
            return -1; // OOM
        }
    }

    uint64_t *rsp = (uint64_t *)(stack + TASK_STACK_SIZE);
    
    /* Set up the initial stack frame matching struct interrupt_frame */
    /* Push order: SS, RSP, RFLAGS, CS, RIP, ERR, INT, GP_REGS... */
    
    *(--rsp) = 0x10; // SS (Kernel Data)
    *(--rsp) = (uint64_t)(stack + TASK_STACK_SIZE - 8); // RSP (ABI requires RSP % 16 == 8 on function entry)
    *(--rsp) = 0x202; // RFLAGS (Interrupts enabled)
    *(--rsp) = 0x08; // CS (Kernel Code)
    *(--rsp) = (uint64_t)entry; // RIP
    *(--rsp) = 0; // ERR_CODE
    *(--rsp) = 0; // INT_NO
    
    /* GP Registers: rax, rbx, rcx, rdx, rsi, rdi, rbp, r8-r15 */
    *(--rsp) = 0; // rax
    *(--rsp) = 0; // rbx
    *(--rsp) = 0; // rcx
    *(--rsp) = 0; // rdx
    *(--rsp) = 0; // rsi
    *(--rsp) = (uint64_t)arg; // rdi (1st argument in System V AMD64 ABI)
    *(--rsp) = 0; // rbp
    *(--rsp) = 0; // r8
    *(--rsp) = 0; // r9
    *(--rsp) = 0; // r10
    *(--rsp) = 0; // r11
    *(--rsp) = 0; // r12
    *(--rsp) = 0; // r13
    *(--rsp) = 0; // r14
    *(--rsp) = 0; // r15

    strncpy(tasks[slot].name, name, 31);
    tasks[slot].name[31] = '\0';
    tasks[slot].rsp = (uint64_t)rsp;
    tasks[slot].stack_base = (uint64_t)stack;
    tasks[slot].task_id = next_task_id++;
    tasks[slot].state = TASK_READY;
    tasks[slot].ticks = 0;
    tasks[slot].cr3 = tasks[0].cr3;
    tasks[slot].priority = PRIO_NORMAL; // Default priority
    
    for (int j = 0; j < MAX_FDS; j++) {
        tasks[slot].fds[j] = NULL;
    }

    int ret_id = tasks[slot].task_id;
    spin_unlock(&scheduler_lock);
    
    struct cpu_local *cpu = cpu_current();
    if (cpu) {
        spin_lock(&cpu->rq_lock);
        if (cpu->rq_count < 32) {
            cpu->rq[cpu->rq_count++] = slot;
        }
        spin_unlock(&cpu->rq_lock);
    }
    
    return ret_id;
}

int task_create_user(void (*entry)(void *), void *arg, const char *name) {
    spin_lock(&scheduler_lock);
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        spin_unlock(&scheduler_lock);
        return -1; // Max tasks reached
    }

    uint8_t *stack;
    if (tasks[slot].stack_base != 0) {
        stack = (uint8_t *)tasks[slot].stack_base;
    } else {
        stack = kmalloc(TASK_STACK_SIZE);
        if (!stack) { spin_unlock(&scheduler_lock); return -1; } // OOM
    }

    uint64_t *rsp = (uint64_t *)(stack + TASK_STACK_SIZE);
    
    *(--rsp) = 0x1B; // SS (User Data, RPL=3)
    *(--rsp) = (uint64_t)(stack + TASK_STACK_SIZE - 8); // RSP (ABI requires RSP % 16 == 8 on function entry)
    *(--rsp) = 0x202; // RFLAGS (Interrupts enabled)
    *(--rsp) = 0x23; // CS (User Code, RPL=3)
    *(--rsp) = (uint64_t)entry; // RIP
    *(--rsp) = 0; // ERR_CODE
    *(--rsp) = 0; // INT_NO
    
    /* GP Registers: rax, rbx, rcx, rdx, rsi, rdi, rbp, r8-r15 */
    *(--rsp) = 0; // rax
    *(--rsp) = 0; // rbx
    *(--rsp) = 0; // rcx
    *(--rsp) = 0; // rdx
    *(--rsp) = 0; // rsi
    *(--rsp) = (uint64_t)arg; // rdi
    *(--rsp) = 0; // rbp
    *(--rsp) = 0; // r8
    *(--rsp) = 0; // r9
    *(--rsp) = 0; // r10
    *(--rsp) = 0; // r11
    *(--rsp) = 0; // r12
    *(--rsp) = 0; // r13
    *(--rsp) = 0; // r14
    *(--rsp) = 0; // r15

    strncpy(tasks[slot].name, name, 31);
    tasks[slot].name[31] = '\0';
    tasks[slot].rsp = (uint64_t)rsp;
    tasks[slot].stack_base = (uint64_t)stack;
    tasks[slot].task_id = next_task_id++;
    tasks[slot].state = TASK_READY;
    tasks[slot].ticks = 0;
    tasks[slot].cr3 = tasks[0].cr3;

    int ret_id = tasks[slot].task_id;
    spin_unlock(&scheduler_lock);
    
    struct cpu_local *cpu = cpu_current();
    if (cpu) {
        spin_lock(&cpu->rq_lock);
        if (cpu->rq_count < 32) {
            cpu->rq[cpu->rq_count++] = slot;
        }
        spin_unlock(&cpu->rq_lock);
    }
    
    return ret_id;
}

int process_create(void (*entry)(void *), const char *name) {
    spin_lock(&scheduler_lock);
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        spin_unlock(&scheduler_lock);
        return -1; // Max tasks reached
    }

    // Create a new PML4 for this process
    extern uint64_t vmm_create_pml4(void);
    uint64_t cr3 = vmm_create_pml4();
    if (!cr3) { spin_unlock(&scheduler_lock); return -1; }
    
    // Allocate Kernel Stack for the process
    uint8_t *kernel_stack;
    if (tasks[slot].stack_base != 0) {
        kernel_stack = (uint8_t *)tasks[slot].stack_base;
    } else {
        kernel_stack = kmalloc(TASK_STACK_SIZE);
        if (!kernel_stack) {
            spin_unlock(&scheduler_lock);
            return -1;
        }
    }
    
    // Map User Stack at 0x700000000000 (4 pages)
    extern uint64_t hhdm_offset;
    extern uint64_t pmm_alloc(void);
    extern void vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
    
    uint64_t user_stack_virt = 0x700000000000;
    for (int i = 0; i < 4; i++) {
        uint64_t phys = pmm_alloc();
        if (phys) {
            vmm_map_page((uint64_t *)(cr3 + hhdm_offset), user_stack_virt - (i * 4096), phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
        } else {
            spin_unlock(&scheduler_lock);
            return -1; // OOM while mapping user stack
        }
    }

    uint64_t *rsp = (uint64_t *)(kernel_stack + TASK_STACK_SIZE);
    
    *(--rsp) = 0x1B; // SS (User Data, RPL=3)
    *(--rsp) = user_stack_virt; // RSP (User Stack)
    *(--rsp) = 0x202; // RFLAGS
    *(--rsp) = 0x23; // CS (User Code, RPL=3)
    *(--rsp) = (uint64_t)entry; // RIP
    *(--rsp) = 0; // ERR_CODE
    *(--rsp) = 0; // INT_NO
    
    // Push GP Registers (15 of them)
    for (int i = 0; i < 15; i++) {
        *(--rsp) = 0;
    }

    strncpy(tasks[slot].name, name, 31);
    tasks[slot].name[31] = '\0';
    tasks[slot].rsp = (uint64_t)rsp;
    tasks[slot].stack_base = (uint64_t)kernel_stack;
    tasks[slot].task_id = next_task_id++;
    tasks[slot].state = TASK_READY;
    tasks[slot].ticks = 0;
    tasks[slot].cr3 = cr3;
    tasks[slot].priority = PRIO_NORMAL;
    
    for (int j = 0; j < MAX_FDS; j++) {
        tasks[slot].fds[j] = NULL;
    }

    int ret_id = tasks[slot].task_id;
    spin_unlock(&scheduler_lock);
    
    struct cpu_local *cpu = cpu_current();
    if (cpu) {
        spin_lock(&cpu->rq_lock);
        if (cpu->rq_count < 32) {
            cpu->rq[cpu->rq_count++] = slot;
        }
        spin_unlock(&cpu->rq_lock);
    }
    
    return ret_id;
}

int process_exec(const char *path, const char *name) {
    if (!path) return -1;
    if (!name) name = path;

    // Resolve full path if relative
    char full_path[VFS_MAX_PATH];
    strncpy(full_path, path, VFS_MAX_PATH - 1);
    full_path[VFS_MAX_PATH - 1] = '\0';

    struct vfs_node *node = vfs_open(full_path);
    if (!node && path[0] != '/') {
        snprintf(full_path, sizeof(full_path), "/hd0/%s", path);
        node = vfs_open(full_path);
        if (!node) {
            snprintf(full_path, sizeof(full_path), "/%s", path);
            node = vfs_open(full_path);
        }
    }

    if (!node) {
        kprintf("[EXEC] Could not find executable: %s\n", path);
        return -1;
    }

    // 1. Create page table PML4 for user process
    extern uint64_t vmm_create_pml4(void);
    extern void vmm_destroy_pml4(uint64_t cr3);
    uint64_t cr3 = vmm_create_pml4();
    if (!cr3) {
        kprintf("[EXEC] vmm_create_pml4 failed for %s\n", full_path);
        return -1;
    }

    // 2. Map User Stack at 0x700000000000 (16KB: 4 pages below user_stack_top)
    extern uint64_t hhdm_offset;
    extern uint64_t pmm_alloc(void);
    extern void vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
    
    uint64_t user_stack_top = 0x700000000000ULL;
    for (int i = 0; i < 4; i++) {
        uint64_t phys = pmm_alloc();
        if (phys) {
            uint64_t stack_page_virt = user_stack_top - ((i + 1) * 4096);
            vmm_map_page((uint64_t *)(cr3 + hhdm_offset), stack_page_virt, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
            memset((void *)(phys + hhdm_offset), 0, 4096);
        } else {
            kprintf("[EXEC] pmm_alloc failed for user stack of %s\n", full_path);
            vmm_destroy_pml4(cr3);
            return -1;
        }
    }

    // 3. Load ELF into user PML4
    uint64_t entry_point = 0;
    extern int elf_load(const char *path, uint64_t *out_entry_point, uint64_t cr3);
    if (elf_load(full_path, &entry_point, cr3) != 0) {
        kprintf("[EXEC] elf_load failed for %s\n", full_path);
        vmm_destroy_pml4(cr3);
        return -1;
    }

    // 4. Allocate Kernel Stack
    uint8_t *kernel_stack = kmalloc(TASK_STACK_SIZE);
    if (!kernel_stack) {
        kprintf("[EXEC] kmalloc failed for kernel stack of %s\n", full_path);
        vmm_destroy_pml4(cr3);
        return -1;
    }

    // 5. Setup interrupt frame for Ring 3 transition on Kernel Stack
    uint64_t *rsp = (uint64_t *)(kernel_stack + TASK_STACK_SIZE);
    
    *(--rsp) = 0x1B; // SS (User Data, RPL=3)
    *(--rsp) = user_stack_top - 8; // RSP (User Stack pointer, 8-byte aligned)
    *(--rsp) = 0x202; // RFLAGS (Interrupts enabled)
    *(--rsp) = 0x23; // CS (User Code, RPL=3)
    *(--rsp) = entry_point; // RIP
    *(--rsp) = 0; // ERR_CODE
    *(--rsp) = 0; // INT_NO
    
    // Push GP Registers (15 of them: rax, rbx, rcx, rdx, rsi, rdi, rbp, r8-r15)
    for (int i = 0; i < 15; i++) {
        *(--rsp) = 0;
    }

    // 6. Acquire scheduler_lock ONLY to assign task slot
    spin_lock(&scheduler_lock);
    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        kprintf("[EXEC] No free task slots for %s\n", full_path);
        spin_unlock(&scheduler_lock);
        kfree(kernel_stack);
        vmm_destroy_pml4(cr3);
        return -1;
    }

    strncpy(tasks[slot].name, name, 31);
    tasks[slot].name[31] = '\0';
    tasks[slot].rsp = (uint64_t)rsp;
    tasks[slot].stack_base = (uint64_t)kernel_stack;
    tasks[slot].task_id = next_task_id++;
    tasks[slot].state = TASK_READY;
    tasks[slot].ticks = 0;
    tasks[slot].cr3 = cr3;
    tasks[slot].priority = PRIO_NORMAL;
    
    for (int j = 0; j < MAX_FDS; j++) {
        tasks[slot].fds[j] = NULL;
    }

    int ret_id = tasks[slot].task_id;
    spin_unlock(&scheduler_lock);
    
    struct cpu_local *cpu = cpu_current();
    if (cpu) {
        spin_lock(&cpu->rq_lock);
        if (cpu->rq_count < 32) {
            cpu->rq[cpu->rq_count++] = slot;
        } else {
            kprintf("[EXEC] CPU run queue is full! Process %s might not run!\n", full_path);
        }
        spin_unlock(&cpu->rq_lock);
    }
    
    kprintf("[EXEC] Process '%s' started with PID %d (CR3=%p, Entry=%p)\n", tasks[slot].name, ret_id, (void*)cr3, (void*)entry_point);
    return ret_id;
}

void task_yield(void) {
    if (!scheduler_enabled) return;
    /* Wait for the next PIT timer interrupt to preempt us safely */
    __asm__ volatile ("sti; hlt");
}

void task_exit(void) {
    struct task *t = current_task();
    if (t) {
        t->state = TASK_ZOMBIE;
    }
    task_yield();
    while(1) {
        __asm__ volatile ("sti; hlt");
    }
}

uint64_t scheduler_switch_task(uint64_t current_rsp) {
    if (!scheduler_enabled) return current_rsp;

    struct cpu_local *cpu = cpu_current();
    if (!cpu || !cpu->current_task) return current_rsp;

    spin_lock(&scheduler_lock);
    
    struct task *curr = cpu->current_task;

    /* Save the current task's state */
    if (curr->state == TASK_RUNNING) {
        curr->rsp = current_rsp;
        curr->state = TASK_READY;
        curr->ticks++;
    }
    
    cpu->scheduler_tick++;
    total_ticks++;
    spin_unlock(&scheduler_lock); // We don't need the global lock for per-cpu queues

    /* Priority Scheduling: Pick highest priority ready task from local run queue */
    struct task *best_task = NULL;
    enum task_priority highest_prio = PRIO_IDLE - 1;

    spin_lock(&cpu->rq_lock);
    for (int i = 0; i < cpu->rq_count; i++) {
        int t_idx = cpu->rq[i];
        if (tasks[t_idx].state == TASK_READY) {
            if (best_task == NULL || tasks[t_idx].priority > highest_prio) {
                best_task = &tasks[t_idx];
                highest_prio = tasks[t_idx].priority;
            }
        }
    }
    spin_unlock(&cpu->rq_lock);

    /* Phase 5: Work Stealing if local queue has no ready tasks */
    if (best_task == NULL) {
        int n_cpus = get_cpu_count();
        for (int c = 0; c < n_cpus; c++) {
            struct cpu_local *other = get_cpu(c);
            if (other == cpu) continue;
            
            if (other->rq_lock.lock == 0) { // Quick check before spinning
                spin_lock(&other->rq_lock);
                for (int i = 0; i < other->rq_count; i++) {
                    int t_idx = other->rq[i];
                    if (tasks[t_idx].state == TASK_READY) {
                        // Steal it!
                        best_task = &tasks[t_idx];
                        // Remove from other
                        for (int j = i; j < other->rq_count - 1; j++) {
                            other->rq[j] = other->rq[j+1];
                        }
                        other->rq_count--;
                        break;
                    }
                }
                spin_unlock(&other->rq_lock);
            }
            if (best_task) {
                // Add to our own queue
                spin_lock(&cpu->rq_lock);
                if (cpu->rq_count < 32) {
                    cpu->rq[cpu->rq_count++] = (int)(best_task - tasks);
                }
                spin_unlock(&cpu->rq_lock);
                break;
            }
        }
    }

    /* If no tasks are ready anywhere, pick the idle task */
    if (best_task == NULL) {
        if (cpu->idle_task && cpu->idle_task->state == TASK_READY) {
            best_task = cpu->idle_task;
        } else {
            return current_rsp;
        }
    }

    /* Switch to the new task */
    cpu->current_task = best_task;
    best_task->state = TASK_RUNNING;
    
    uint64_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
    if (current_cr3 != best_task->cr3) {
        __asm__ volatile("mov %0, %%cr3" : : "r"(best_task->cr3));
    }
    
    /* Update TSS for ring 3 interrupts */
    extern void gdt_set_tss_rsp0(uint64_t rsp0);
    gdt_set_tss_rsp0(best_task->stack_base + TASK_STACK_SIZE);

    return best_task->rsp;
}

const char *scheduler_get_current_name(void) {
    struct task *t = current_task();
    if (t) {
        return t->name;
    }
    return "kernel";
}

int scheduler_get_task_cpu_usage(int index) {
    if (index < 0 || index >= MAX_TASKS || total_ticks == 0) return 0;
    if (tasks[index].state == TASK_DEAD) return 0;
    return (tasks[index].ticks * 100) / total_ticks;
}

/* File Descriptor Management API for Syscalls */
int scheduler_allocate_fd(struct vfs_node *node) {
    struct task *t = current_task();
    if (!t) return -1;
    
    spin_lock(&scheduler_lock);
    // Reserve 0, 1, 2 for stdin, stdout, stderr
    for (int i = 3; i < MAX_FDS; i++) {
        if (t->fds[i] == NULL) {
            struct file_descriptor *fd_obj = (struct file_descriptor *)kmalloc(sizeof(struct file_descriptor));
            if (!fd_obj) { spin_unlock(&scheduler_lock); return -1; }
            fd_obj->node = node;
            fd_obj->offset = 0;
            fd_obj->flags = 0;
            t->fds[i] = fd_obj;
            spin_unlock(&scheduler_lock);
            return i;
        }
    }
    spin_unlock(&scheduler_lock);
    return -1; // Max FDs reached
}

struct file_descriptor *scheduler_get_fd(int fd) {
    struct task *t = current_task();
    if (!t) return NULL;
    if (fd < 0 || fd >= MAX_FDS) return NULL;
    return t->fds[fd];
}

void scheduler_free_fd(int fd) {
    struct task *t = current_task();
    if (!t) return;
    if (fd < 0 || fd >= MAX_FDS) return;
    
    spin_lock(&scheduler_lock);
    if (t->fds[fd]) {
        kfree(t->fds[fd]);
        t->fds[fd] = NULL;
    }
    spin_unlock(&scheduler_lock);
}

int scheduler_get_task_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_DEAD) count++;
    }
    return count;
}

void scheduler_get_task_info(int index, int *id, char *name, int *state) {
    int current = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_DEAD) {
            if (current == index) {
                if (id) *id = tasks[i].task_id;
                if (name) strcpy(name, tasks[i].name);
                if (state) *state = tasks[i].state;
                return;
            }
            current++;
        }
    }
}

extern int gui_cpu_usage;

int scheduler_get_cpu_usage(void) {
    int cores = get_cpu_count();
    if (cores <= 0) return gui_cpu_usage;
    
    int total = 0;
    for (int i = 0; i < cores; i++) {
        total += scheduler_get_core_usage(i);
    }
    return total / cores;
}

int scheduler_get_core_count(void) {
    return get_cpu_count();
}

int scheduler_get_core_usage(int core_id) {
    if (core_id == 0) return gui_cpu_usage;
    
    struct cpu_local *cpu = get_cpu(core_id);
    if (!cpu || !cpu->idle_task) return 0;
    
    static uint64_t last_total[MAX_CPUS] = {0};
    static uint64_t last_idle[MAX_CPUS] = {0};
    static int last_usage[MAX_CPUS] = {0};
    
    uint64_t current_total = cpu->scheduler_tick;
    uint64_t current_idle = cpu->idle_task->ticks;
    
    if (current_total > last_total[core_id] && (current_total - last_total[core_id]) >= 10) {
        uint64_t total_diff = current_total - last_total[core_id];
        uint64_t idle_diff = (current_idle >= last_idle[core_id]) ? (current_idle - last_idle[core_id]) : 0;
        
        last_total[core_id] = current_total;
        last_idle[core_id] = current_idle;
        
        if (idle_diff > total_diff) idle_diff = total_diff;
        
        int idle_pct = (int)((idle_diff * 100) / total_diff);
        last_usage[core_id] = 100 - idle_pct;
    }
    
    return last_usage[core_id];
}

uint64_t scheduler_mmap(uint64_t req_addr, size_t length, int prot) {
    (void)prot; // Will be used later to set specific page permissions
    spin_lock(&scheduler_lock);
    
    if (length % 4096 != 0) {
        length = (length / 4096 + 1) * 4096;
    }
    
    struct cpu_local *cpu = cpu_current();
    if (!cpu || cpu->current_task == NULL) {
        spin_unlock(&scheduler_lock);
        return 0;
    }
    
    struct task *t = cpu->current_task;
    
    if (req_addr == 0) {
        if (t->heap_end == 0) t->heap_end = 0x400000000000;
        req_addr = t->heap_end;
        t->heap_end += length;
    }
    
    extern uint64_t hhdm_offset;
    extern uint64_t pmm_alloc(void);
    extern void vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
    
    uint64_t num_pages = length / 4096;
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t phys = pmm_alloc();
        if (!phys) {
            spin_unlock(&scheduler_lock);
            return 0; 
        }
        vmm_map_page((uint64_t *)(t->cr3 + hhdm_offset), req_addr + (i * 4096), phys, 0x07); 
    }
    
    spin_unlock(&scheduler_lock);
    return req_addr;
}

bool task_kill(int task_id) {
    if (task_id <= 1) return false;
    spin_lock(&scheduler_lock);
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_DEAD && tasks[i].task_id == task_id) {
            tasks[i].state = TASK_DEAD;
            spin_unlock(&scheduler_lock);
            return true;
        }
    }
    spin_unlock(&scheduler_lock);
    return false;
}
