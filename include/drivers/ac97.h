#ifndef DRIVERS_AC97_H
#define DRIVERS_AC97_H

#include "zarqaa/types.h"

void ac97_init(void);
void ac97_set_volume(uint8_t volume);

uint32_t ac97_get_ring_buffer_phys(void);
uint8_t *ac97_get_ring_buffer_virt(void);

void ac97_start_ring(void);
void ac97_set_lvi(uint8_t lvi);
uint8_t ac97_get_civ(void);
void ac97_stop(void);

#endif
