/*
 * Zarqaa OS 0.1 - Physical Memory Manager Header
 * Creator: Abdelhak Chabbi
 */

#ifndef KERNEL_PMM_H
#define KERNEL_PMM_H

#include "zarqaa/types.h"
#include "limine.h"

#define PAGE_SIZE 4096

void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset);
uint64_t pmm_alloc(void);
uint64_t pmm_alloc_blocks(uint64_t count);
uint64_t pmm_alloc_blocks_low32(uint64_t count);
void pmm_free(uint64_t addr);
uint64_t pmm_alloc_low32(void);  /* Alloc a page below 4GB for DMA */
uint64_t pmm_get_total(void);
uint64_t pmm_get_used(void);
uint64_t pmm_get_free(void);

#endif /* KERNEL_PMM_H */
