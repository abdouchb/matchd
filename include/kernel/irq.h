/*
 * Zarqaa OS 0.1 - IRQ Subsystem Interface
 * Creator: Abdelhak Chabbi & AI
 */

#pragma once

#include <stdint.h>
#include "kernel/idt.h" // For struct interrupt_frame

/*
 * IRQ handler signature.
 * Returns the new stack pointer (RSP) if a task switch occurred, or 0 if no switch is needed.
 */
typedef uint64_t (*irq_handler_t)(struct interrupt_frame *frame);

void register_irq_handler(uint8_t irq, irq_handler_t handler);
void unregister_irq_handler(uint8_t irq);
irq_handler_t get_irq_handler(uint8_t irq);
