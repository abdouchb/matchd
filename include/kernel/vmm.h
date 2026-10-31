/*
 * Zarqaa OS 0.1 - Virtual Memory Manager Header
 * Creator: Abdelhak Chabbi
 */

#ifndef KERNEL_VMM_H
#define KERNEL_VMM_H

#include "zarqaa/types.h"

#define PTE_PRESENT       (1ull << 0)
#define PTE_WRITABLE      (1ull << 1)
#define PTE_USER          (1ull << 2)
#define PTE_WRITE_THROUGH (1ull << 3)
#define PTE_CACHE_DISABLE (1ull << 4)
#define PTE_HUGE          (1ULL << 7)
#define PTE_NX            (1ull << 63)

#define PML4_INDEX(virt) (((virt) >> 39) & 0x1FF)
#define PDPT_INDEX(virt) (((virt) >> 30) & 0x1FF)
#define PD_INDEX(virt)   (((virt) >> 21) & 0x1FF)
#define PT_INDEX(virt)   (((virt) >> 12) & 0x1FF)

void vmm_init(void);
void vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_map_mmio(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap_page(uint64_t *pml4, uint64_t virt);
bool vmm_is_mapped(uint64_t *pml4, uint64_t virt);
uint64_t vmm_create_pml4(void);
void vmm_destroy_pml4(uint64_t cr3);
extern uint64_t *kernel_pml4;

#endif /* KERNEL_VMM_H */
