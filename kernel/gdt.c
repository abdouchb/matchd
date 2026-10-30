/*
 * Zarqaa OS 0.1 - Global Descriptor Table
 * Creator: Abdelhak Chabbi
 */

#include "kernel/gdt.h"

struct __attribute__((packed)) GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
};

struct __attribute__((packed)) GDTR {
    uint16_t limit;
    uint64_t base;
};

struct __attribute__((packed)) TSS {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
};

static struct GDTEntry gdt[7]; /* Increased to 7 for TSS */
static struct GDTR gdtr;
static struct TSS tss = {0};

/* External function to set the kernel stack pointer for Ring 3 -> Ring 0 transitions */
void gdt_set_tss_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

void gdt_init(void) {
    /* 0: Null descriptor */
    gdt[0] = (struct GDTEntry){0, 0, 0, 0x00, 0x00, 0};

    /* 1: Kernel Code (0x08) */
    gdt[1] = (struct GDTEntry){0xFFFF, 0, 0, 0x9A, 0x20, 0};

    /* 2: Kernel Data (0x10) */
    gdt[2] = (struct GDTEntry){0xFFFF, 0, 0, 0x92, 0x00, 0};

    /* 3: User Data (0x18) */
    gdt[3] = (struct GDTEntry){0xFFFF, 0, 0, 0xF2, 0x00, 0};

    /* 4: User Code (0x20) */
    gdt[4] = (struct GDTEntry){0xFFFF, 0, 0, 0xFA, 0x20, 0};

    /* 5 and 6: TSS Descriptor (16 bytes in x86_64) */
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;
    
    gdt[5] = (struct GDTEntry){
        .limit_low = tss_limit & 0xFFFF,
        .base_low = tss_base & 0xFFFF,
        .base_mid = (tss_base >> 16) & 0xFF,
        .access = 0x89, /* Present, Ring 0, TSS (Available) */
        .granularity = (tss_limit >> 16) & 0x0F,
        .base_high = (tss_base >> 24) & 0xFF
    };
    
    /* The high 8 bytes of a 64-bit TSS descriptor */
    gdt[6] = (struct GDTEntry){
        .limit_low = (tss_base >> 32) & 0xFFFF,
        .base_low = (tss_base >> 48) & 0xFFFF,
        .base_mid = 0,
        .access = 0,
        .granularity = 0,
        .base_high = 0
    };

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt;

    __asm__ volatile("lgdt %0" : : "m"(gdtr));

    __asm__ volatile(
        "push $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "push %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov $0x00, %%ax\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        ::: "rax", "memory"
    );
    
    __asm__ volatile("ltr %0" : : "r"((uint16_t)0x28));
}

void gdt_reload(void) {
    __asm__ volatile("lgdt %0" : : "m"(gdtr));
    __asm__ volatile(
        "push $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "push %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov $0x00, %%ax\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        ::: "rax", "memory"
    );
    __asm__ volatile("ltr %0" : : "r"((uint16_t)0x28));
}

void gdt_reload_ap(void) {
    __asm__ volatile("lgdt %0" : : "m"(gdtr));
    __asm__ volatile(
        "push $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "push %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov $0x00, %%ax\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        ::: "rax", "memory"
    );
    /* Do NOT execute ltr here! 
     * The global TSS descriptor is already marked BUSY by the BSP.
     * Executing ltr on a BUSY TSS causes a #GP fault, which triple-faults APs!
     */
}
