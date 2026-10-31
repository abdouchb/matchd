/*
 * Zarqaa OS - Symmetric Multiprocessing (SMP) Initialization
 * 
 * Phase 1: Bring all Application Processors (APs) online.
 */

#include "kernel/smp.h"
#include "kernel/cpu_local.h"
#include "limine.h"
#include "zarqaa/types.h"
#include "kernel/pmm.h"
#include "kernel/early_boot_log.h"
#include "zarqaa/string.h"
#include "kernel/apic.h"

extern uint64_t hhdm_offset;

/* Parallel job pool: APs sleep in hlt and are woken by IPI 253 (the
 * otherwise-harmless TLB shootdown vector) when the BSP posts a job. */
#define SMP_JOB_VECTOR 253

static struct {
    void (*fn)(void *);
    void *arg;
} job;
static uint32_t job_gen;      /* bumped (release) after fn/arg are set */
static uint32_t job_done;     /* workers finished with the current job */
static uint32_t workers_ready;
static uint32_t pool_lock;

static void pool_lock_acquire(void) {
    while (__atomic_exchange_n(&pool_lock, 1, __ATOMIC_ACQUIRE))
        __asm__ volatile("pause");
}

static void pool_lock_release(void) {
    __atomic_store_n(&pool_lock, 0, __ATOMIC_RELEASE);
}

int smp_worker_count(void) {
    return (int)__atomic_load_n(&workers_ready, __ATOMIC_ACQUIRE);
}

void smp_parallel_run(void (*fn)(void *), void *arg) {
    if (smp_worker_count() == 0) {
        fn(arg);
        return;
    }

    pool_lock_acquire();
    uint32_t n = __atomic_load_n(&workers_ready, __ATOMIC_ACQUIRE);
    job.fn = fn;
    job.arg = arg;
    __atomic_store_n(&job_done, 0, __ATOMIC_RELAXED);
    __atomic_add_fetch(&job_gen, 1, __ATOMIC_RELEASE);
    apic_broadcast_ipi(SMP_JOB_VECTOR, true);

    fn(arg);

    while (__atomic_load_n(&job_done, __ATOMIC_ACQUIRE) != n)
        __asm__ volatile("pause");
    pool_lock_release();
}

static void ap_enable_sse(void) {
    uint64_t cr0, cr4;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);
    cr0 |= (1ULL << 1);
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10);
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
    __asm__ volatile("fninit");
}

/* Limine SMP request (must be in .limine_requests section) */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_smp_request smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0,
    .response = NULL,
    .flags = LIMINE_SMP_X2APIC
};

/**
 * @brief AP entry point
 * 
 * This is where Limine jumps all secondary CPU cores.
 * We must not return from this function.
 */
static void ap_start(struct limine_smp_info *info) {
    /* Load Kernel GDT and IDT for this AP so it can handle interrupts */
    extern void gdt_reload_ap(void);
    extern void idt_reload(void);
    gdt_reload_ap();
    idt_reload();

    /* Initialize per-CPU data for AP (MUST BE AFTER gdt_reload_ap because mov to gs clears GS_BASE!) */
    cpu_local_init(info->lapic_id);
    
    /* Initialize LAPIC for this CPU core */
    extern void lapic_init(void);
    lapic_init();
    
    /* Setup scheduler idle task for this AP */
    extern void scheduler_ap_setup(void);
    scheduler_ap_setup();

    /* Print that this CPU is online */
    kprintf("[SMP] CPU %u online\n", info->lapic_id);
    
    ap_enable_sse();

    /* Registering under pool_lock guarantees we are either counted in a
     * running job or start after it (seen == its generation). */
    pool_lock_acquire();
    uint32_t seen = __atomic_load_n(&job_gen, __ATOMIC_ACQUIRE);
    __atomic_add_fetch(&workers_ready, 1, __ATOMIC_RELEASE);
    pool_lock_release();

    /* Interrupts stay off between jobs; "sti; hlt" is atomic so a job
     * IPI can't be missed. */
    while (1) {
        __asm__ volatile("cli");
        uint32_t gen = __atomic_load_n(&job_gen, __ATOMIC_ACQUIRE);
        if (gen != seen) {
            seen = gen;
            job.fn(job.arg);
            __atomic_add_fetch(&job_done, 1, __ATOMIC_RELEASE);
            continue;
        }
        __asm__ volatile("sti; hlt");
    }
}

/**
 * @brief Initialize SMP subsystem
 */
void smp_init(void) {
    if (smp_request.response == NULL) {
        kprintf("[SMP] Bootloader did not provide SMP info.\n");
        return;
    }
    
    uint64_t cpu_count = smp_request.response->cpu_count;
    kprintf("[SMP] Found %lu CPUs.\n", (unsigned long)cpu_count);

    for (uint64_t i = 0; i < cpu_count; i++) {
        struct limine_smp_info *cpu = smp_request.response->cpus[i];
        
        /* The Bootstrap Processor (BSP) is already running this code, skip it */
        if (cpu->lapic_id == smp_request.response->bsp_lapic_id) {
            kprintf("[SMP] CPU %u is the BSP.\n", cpu->lapic_id);
            /* Initialize per-CPU data for the BSP */
            cpu_local_init(cpu->lapic_id);
            continue;
        }
        
        /* For APs, we tell Limine where to jump to start executing kernel code */
        cpu->goto_address = ap_start;
    }
    
    kprintf("[SMP] All APs have been signaled to start.\n");
}
