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

extern uint64_t hhdm_offset;

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
    
    /* Enable interrupts and halt. The timer will trigger scheduling. */
    while (1) {
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
