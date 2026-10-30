/*
 * Zarqaa OS 0.1 - Programmable Interval Timer Driver
 * Creator: Abdelhak Chabbi
 */

#include "drivers/pit.h"
#include "kernel/scheduler.h"

#define PIT_CH0      0x40
#define PIT_CMD      0x43
#define PIT_BASE_FREQ 1193182

static uint64_t ticks = 0;
static uint32_t current_freq = 100;

void pit_init(uint32_t frequency) {
    current_freq = frequency;
    uint32_t divisor = PIT_BASE_FREQ / frequency;
    
    outb(PIT_CMD, 0x36); /* Channel 0, lobyte/hibyte, Mode 3 (Square Wave), Binary */
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
}

uint64_t pit_handler(struct interrupt_frame *frame) {
    ticks++;
    return scheduler_switch_task((uint64_t)frame);
}

uint64_t pit_get_ticks(void) {
    return ticks;
}

void pit_sleep_ms(uint32_t ms) {
    uint64_t target_ticks = ticks + (ms * current_freq) / 1000;
    while (ticks < target_ticks) {
        hlt();
    }
}

uint32_t pit_get_seconds(void) {
    return (uint32_t)(ticks / current_freq);
}
