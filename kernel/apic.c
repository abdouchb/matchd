#include "kernel/apic.h"
#include "kernel/pic.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/early_boot_log.h"
#include "zarqaa/string.h"
#include "kernel/cpu_local.h"
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/acpi.h>

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

/* LAPIC Registers (offsets) */
#define LAPIC_ID         0x020
#define LAPIC_VER        0x030
#define LAPIC_TPR        0x080
#define LAPIC_EOI        0x0B0
#define LAPIC_SVR        0x0F0
#define LAPIC_SPURIOUS   0xFF
#define LAPIC_TIMER      0x320
#define LAPIC_LINT0      0x350
#define LAPIC_LINT1      0x360
#define LAPIC_TIMER_INIT 0x380
#define LAPIC_TIMER_CUR  0x390
#define LAPIC_TIMER_DIV  0x3E0
#define LAPIC_ICR_LOW    0x300
#define LAPIC_ICR_HIGH   0x310

static uint64_t lapic_base_phys = 0;
static uint8_t *lapic_base_virt = NULL;

static uint64_t ioapic_base_phys = 0;
static uint8_t *ioapic_base_virt = NULL;

static uint32_t iso_gsi[16];
static uint16_t iso_flags[16];

static void lapic_write(uint32_t reg, uint32_t val) {
    if (!lapic_base_virt) return;
    volatile uint32_t *ptr = (volatile uint32_t *)(lapic_base_virt + reg);
    *ptr = val;
}

static uint32_t lapic_read(uint32_t reg) {
    if (!lapic_base_virt) return 0;
    volatile uint32_t *ptr = (volatile uint32_t *)(lapic_base_virt + reg);
    return *ptr;
}

static void ioapic_write(uint32_t reg, uint32_t val) {
    if (!ioapic_base_virt) return;
    volatile uint32_t *ioreg = (volatile uint32_t *)(ioapic_base_virt + 0x00);
    volatile uint32_t *iodat = (volatile uint32_t *)(ioapic_base_virt + 0x10);
    *ioreg = reg;
    *iodat = val;
}

static uint32_t ioapic_read(uint32_t reg) {
    if (!ioapic_base_virt) return 0;
    volatile uint32_t *ioreg = (volatile uint32_t *)(ioapic_base_virt + 0x00);
    volatile uint32_t *iodat = (volatile uint32_t *)(ioapic_base_virt + 0x10);
    *ioreg = reg;
    return *iodat;
}

void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

uint32_t lapic_id(void) {
    return lapic_read(LAPIC_ID) >> 24;
}

void apic_send_ipi(uint32_t apic_id, uint8_t vector) {
    lapic_write(LAPIC_ICR_HIGH, (apic_id << 24));
    lapic_write(LAPIC_ICR_LOW, vector | 0x4000); // 0x4000 = Assert, Fixed delivery
}

void apic_broadcast_ipi(uint8_t vector, bool exclude_self) {
    uint32_t shorthand = exclude_self ? 0xC0000 : 0x80000;
    lapic_write(LAPIC_ICR_LOW, vector | shorthand | 0x4000);
}

void lapic_init(void) {
    uint64_t base = rdmsr(IA32_APIC_BASE_MSR);
    /* Enable LAPIC globally if not enabled */
    if (!(base & IA32_APIC_BASE_MSR_ENABLE)) {
        wrmsr(IA32_APIC_BASE_MSR, base | IA32_APIC_BASE_MSR_ENABLE);
        base = rdmsr(IA32_APIC_BASE_MSR);
    }

    if (lapic_base_phys == 0) {
        lapic_base_phys = base & 0xFFFFF000;
    }
    
    /* LAPIC is MMIO — must explicitly map it (not covered by HHDM) */
    extern uint64_t hhdm_offset;
    extern uint64_t *kernel_pml4;
    lapic_base_virt = (uint8_t *)(lapic_base_phys + hhdm_offset);
    vmm_map_mmio(kernel_pml4, (uint64_t)lapic_base_virt, lapic_base_phys,
                 PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE);

    /* Enable Local APIC (Software Enable) and set spurious vector */
    lapic_write(LAPIC_SVR, LAPIC_SPURIOUS | 0x100);

    /* Setup LINT0 as ExtINT to preserve legacy PIC routing just in case */
    lapic_write(LAPIC_LINT0, 0x0700);
    /* Setup LINT1 as NMI */
    lapic_write(LAPIC_LINT1, 0x0400);
    
    /* Clear Task Priority Register to accept all interrupts */
    lapic_write(LAPIC_TPR, 0);
    
    /* Acknowledge any outstanding interrupts */
    lapic_eoi();
}

static uacpi_iteration_decision parse_madt_entry(void *ctx, struct acpi_entry_hdr *hdr) {
    (void)ctx;
    
    if (hdr->type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
        struct acpi_madt_ioapic *ioapic = (struct acpi_madt_ioapic *)hdr;
        if (ioapic_base_phys == 0) { // Only grab the first one for basic routing
            ioapic_base_phys = ioapic->address;
        }
    } else if (hdr->type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
        struct acpi_madt_interrupt_source_override *iso = (struct acpi_madt_interrupt_source_override *)hdr;
        if (iso->source < 16) {
            iso_gsi[iso->source] = iso->gsi;
            iso_flags[iso->source] = iso->flags;
        }
    }
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void ioapic_set_entry(uint8_t gsi, uint8_t vector, uint16_t flags, uint32_t apic_id) {
    uint32_t low_index = 0x10 + gsi * 2;
    uint32_t high_index = 0x10 + gsi * 2 + 1;

    /* Start UNMASKED so IRQs fire immediately when apic_init is done */
    uint32_t low = vector; // NOT masked
    
    // Polarity
    if ((flags & 3) == 3) low |= (1 << 13); // Active Low
    else if ((flags & 3) == 1) low |= (0 << 13); // Active High
    
    // Trigger Mode
    if (((flags >> 2) & 3) == 3) low |= (1 << 15); // Level Triggered
    else if (((flags >> 2) & 3) == 1) low |= (0 << 15); // Edge Triggered

    ioapic_write(low_index, low);
    ioapic_write(high_index, apic_id << 24);
}

void ioapic_unmask(uint8_t irq) {
    if (!ioapic_base_virt || irq >= 16) return;
    uint32_t gsi = iso_gsi[irq];
    uint32_t low_index = 0x10 + gsi * 2;
    uint32_t low = ioapic_read(low_index);
    low &= ~(1 << 16); // Clear mask bit
    ioapic_write(low_index, low);
}

void ioapic_mask(uint8_t irq) {
    if (!ioapic_base_virt || irq >= 16) return;
    uint32_t gsi = iso_gsi[irq];
    uint32_t low_index = 0x10 + gsi * 2;
    uint32_t low = ioapic_read(low_index);
    low |= (1 << 16); // Set mask bit
    ioapic_write(low_index, low);
}

void apic_init(void) {
    kprintf("[APIC] Initializing Advanced Programmable Interrupt Controller...\n");
    
    /* 1. Disable the 8259 PIC completely */
    pic_mask_all();
    for (int i = 0; i < 16; i++) {
        iso_gsi[i] = i;
        iso_flags[i] = 0; // Conforming
    }

    /* 1. Disable legacy PIC (8259A) since we are using APIC */
    extern void pic_mask_all(void);
    pic_mask_all();

    /* 2. Find LAPIC and IOAPIC bases from ACPI MADT */
    uacpi_table tbl;
    if (uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &tbl) == UACPI_STATUS_OK) {
        struct acpi_madt *madt = (struct acpi_madt *)tbl.hdr;
        lapic_base_phys = madt->local_interrupt_controller_address;
        
        uacpi_for_each_subtable(tbl.hdr, sizeof(struct acpi_madt), parse_madt_entry, NULL);
        uacpi_table_unref(&tbl);
    }
    
    /* 4. Initialize LAPIC on BSP */
    lapic_init();
    
    /* 5. IOAPIC routing with ISOs and round-robin distribution */
    if (ioapic_base_phys != 0) {
        extern uint64_t hhdm_offset;
        extern uint64_t *kernel_pml4;
        ioapic_base_virt = (uint8_t *)(ioapic_base_phys + hhdm_offset);
        
        /* IOAPIC is MMIO — must explicitly map it */
        vmm_map_mmio(kernel_pml4, (uint64_t)ioapic_base_virt, ioapic_base_phys,
                     PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE);
        
        /* Route all legacy ISA IRQs (0-15) directly to the Bootstrap Processor (BSP)
         * to ensure timer ticks and input interrupts wake the main kernel loop. */
        uint32_t bsp_apic_id = lapic_id();
        
        for (int irq = 0; irq <= 15; irq++) {
            if (irq == 2) continue; // Skip cascade
            
            uint32_t gsi = iso_gsi[irq];
            uint16_t flags = iso_flags[irq];
            
            ioapic_set_entry(gsi, 32 + irq, flags, bsp_apic_id);
        }
    }
    
    /* Ensure critical hardware IRQs are unmasked:
     * IRQ 0 = PIT Timer (drives scheduler, timekeeping, frame pacing)
     * IRQ 1 = Keyboard (PS/2 / USB Legacy)
     * IRQ 12 = Mouse / Trackpad (PS/2 / USB Legacy)
     */
    extern void *get_irq_handler(uint8_t irq);
    for (int irq = 0; irq < 16; irq++) {
        if (irq == 2) continue; // Skip cascade
        if (irq == 0 || irq == 1 || irq == 12 || get_irq_handler(irq) != 0) {
            ioapic_unmask(irq);
        } else {
            ioapic_mask(irq);
        }
    }

    kprintf("[APIC] LAPIC and IOAPIC initialized. IRQs routed to BSP.\n");
}

/* LAPIC Timer removed in favor of Legacy PIT via IOAPIC */
void lapic_timer_init(void) {
    // Disabled
}
