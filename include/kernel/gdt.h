/*
 * Zarqaa OS 0.1 - Global Descriptor Table Header
 * Creator: Abdelhak Chabbi
 */

#ifndef KERNEL_GDT_H
#define KERNEL_GDT_H

#include "zarqaa/types.h"

#define KERNEL_CODE_SEG 0x08
#define KERNEL_DATA_SEG 0x10
#define USER_DATA_SEG   0x18
#define USER_CODE_SEG   0x20

void gdt_init(void);
void gdt_set_tss_rsp0(uint64_t rsp0);

#endif /* KERNEL_GDT_H */
