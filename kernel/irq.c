/*
 * Zarqaa OS 0.1 - Dynamic IRQ Subsystem
 * Creator: Abdelhak Chabbi & AI
 */

#include "kernel/irq.h"

// 16 hardware IRQs from PIC
static irq_handler_t irq_handlers[16] = {0};

void register_irq_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
        extern void ioapic_unmask(uint8_t irq);
        ioapic_unmask(irq);
    }
}

void unregister_irq_handler(uint8_t irq) {
    if (irq < 16) {
        irq_handlers[irq] = 0;
        extern void ioapic_mask(uint8_t irq);
        ioapic_mask(irq);
    }
}

irq_handler_t get_irq_handler(uint8_t irq) {
    if (irq < 16) {
        return irq_handlers[irq];
    }
    return 0;
}
