#ifndef APIC_H
#define APIC_H

#include <stdint.h>
#include <stdbool.h>

void apic_init(void);
void lapic_init(void);
void lapic_timer_init(void);
void lapic_eoi(void);
uint32_t lapic_id(void);
void apic_send_ipi(uint32_t apic_id, uint8_t vector);
void apic_broadcast_ipi(uint8_t vector, bool exclude_self);

#endif
