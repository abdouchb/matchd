/*
 * Zarqaa OS 0.1 - Programmable Interval Timer Header
 * Creator: Abdelhak Chabbi
 */

#ifndef DRIVERS_PIT_H
#define DRIVERS_PIT_H

#include "zarqaa/types.h"

#include "kernel/idt.h"

void pit_init(uint32_t frequency);
uint64_t pit_handler(struct interrupt_frame *frame);
uint64_t pit_get_ticks(void);
void pit_sleep_ms(uint32_t ms);
uint32_t pit_get_seconds(void);

#endif /* DRIVERS_PIT_H */
