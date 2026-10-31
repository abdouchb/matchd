/*
 * Zarqaa OS 0.1 - PIC (Programmable Interrupt Controller) Header
 * Creator: Abdelhak Chabbi
 */

#ifndef KERNEL_PIC_H
#define KERNEL_PIC_H

#include "zarqaa/types.h"

void pic_init(void);
void pic_eoi(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);
void pic_mask_all(void);

#endif /* KERNEL_PIC_H */
