/*
 * Zarqaa OS 0.1 - Virtual Memory Manager
 * Creator: Abdelhak Chabbi
 */

#include "kernel/vmm.h"
#include "kernel/pmm.h"
#include "kernel/spinlock.h"
#include "zarqaa/string.h"

extern uint64_t hhdm_offset;
uint64_t *kernel_pml4 = NULL;
static spinlock_t vmm_lock = SPINLOCK_INIT;

void vmm_init(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    kernel_pml4 = (uint64_t *)((cr3 & ~0xFFF) + hhdm_offset);
}

static uint64_t *get_next_level(uint64_t *table, uint64_t index) {
    if ((table[index] & PTE_PRESENT) != 0) {
        if ((table[index] & PTE_HUGE) != 0) {
            /* Error: Attempted to traverse a Huge Page as a page table! */
            return NULL;
        }
        return (uint64_t *)((table[index] & ~0xFFF & ~PTE_NX) + hhdm_offset);
    }

    uint64_t phys = pmm_alloc();
    if (phys == 0) return NULL; // OOM

    uint64_t *virt = (uint64_t *)(phys + hhdm_offset);
    memset(virt, 0, PAGE_SIZE);

    table[index] = phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    return virt;
}

void vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    spin_lock(&vmm_lock);
    
    uint64_t *pdpt = get_next_level(pml4, pml4_idx);
    if (!pdpt) { spin_unlock(&vmm_lock); return; }

    uint64_t *pd = get_next_level(pdpt, pdpt_idx);
    if (!pd) { spin_unlock(&vmm_lock); return; }

    uint64_t *pt = get_next_level(pd, pd_idx);
    if (!pt) { spin_unlock(&vmm_lock); return; }

    pt[pt_idx] = phys | flags;

    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
    spin_unlock(&vmm_lock);
}

void vmm_map_mmio(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    spin_lock(&vmm_lock);

    if ((pml4[pml4_idx] & PTE_PRESENT) == 0) {
        uint64_t p = pmm_alloc();
        if (!p) { spin_unlock(&vmm_lock); return; }
        memset((void*)(p + hhdm_offset), 0, PAGE_SIZE);
        pml4[pml4_idx] = p | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    uint64_t *pdpt = (uint64_t *)((pml4[pml4_idx] & ~0xFFF & ~PTE_NX) + hhdm_offset);

    if ((pdpt[pdpt_idx] & PTE_PRESENT) != 0 && (pdpt[pdpt_idx] & PTE_HUGE) != 0) {
        /* Already mapped by a 1GB huge page (e.g. by Limine) */
        spin_unlock(&vmm_lock);
        return;
    }
    if ((pdpt[pdpt_idx] & PTE_PRESENT) == 0) {
        uint64_t p = pmm_alloc();
        if (!p) { spin_unlock(&vmm_lock); return; }
        memset((void*)(p + hhdm_offset), 0, PAGE_SIZE);
        pdpt[pdpt_idx] = p | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    uint64_t *pd = (uint64_t *)((pdpt[pdpt_idx] & ~0xFFF & ~PTE_NX) + hhdm_offset);

    if ((pd[pd_idx] & PTE_PRESENT) != 0 && (pd[pd_idx] & PTE_HUGE) != 0) {
        /* Already mapped by a 2MB huge page (e.g. by Limine) */
        spin_unlock(&vmm_lock);
        return;
    }
    if ((pd[pd_idx] & PTE_PRESENT) == 0) {
        uint64_t p = pmm_alloc();
        if (!p) { spin_unlock(&vmm_lock); return; }
        memset((void*)(p + hhdm_offset), 0, PAGE_SIZE);
        pd[pd_idx] = p | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    uint64_t *pt = (uint64_t *)((pd[pd_idx] & ~0xFFF & ~PTE_NX) + hhdm_offset);

    if (!pt) { spin_unlock(&vmm_lock); return; }

    pt[pt_idx] = phys | flags;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
    spin_unlock(&vmm_lock);
}

void vmm_unmap_page(uint64_t *pml4, uint64_t virt) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    spin_lock(&vmm_lock);

    if ((pml4[pml4_idx] & PTE_PRESENT) == 0) { spin_unlock(&vmm_lock); return; }
    uint64_t *pdpt = (uint64_t *)((pml4[pml4_idx] & ~0xFFF & ~PTE_NX) + hhdm_offset);

    if ((pdpt[pdpt_idx] & PTE_PRESENT) == 0) { spin_unlock(&vmm_lock); return; }
    uint64_t *pd = (uint64_t *)((pdpt[pdpt_idx] & ~0xFFF & ~PTE_NX) + hhdm_offset);

    if ((pd[pd_idx] & PTE_PRESENT) == 0) { spin_unlock(&vmm_lock); return; }
    uint64_t *pt = (uint64_t *)((pd[pd_idx] & ~0xFFF & ~PTE_NX) + hhdm_offset);

    if ((pt[pt_idx] & PTE_PRESENT) == 0) { spin_unlock(&vmm_lock); return; }

    // Free the underlying physical memory
    uint64_t phys = pt[pt_idx] & ~0xFFF & ~PTE_NX;
    pmm_free(phys);

    pt[pt_idx] = 0;

    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
    
    /* Broadcast TLB Shootdown to other CPUs */
    extern void apic_broadcast_ipi(uint8_t vector, _Bool exclude_self);
    apic_broadcast_ipi(253, 1);
    
    spin_unlock(&vmm_lock);
}

bool vmm_is_mapped(uint64_t *pml4, uint64_t virt) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    if (!(pml4[pml4_idx] & PTE_PRESENT)) return false;
    uint64_t *pdpt = (uint64_t *)((pml4[pml4_idx] & ~0xFFF & ~PTE_NX) + hhdm_offset);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return false;
    if (pdpt[pdpt_idx] & PTE_HUGE) return true; // 1GB page
    uint64_t *pd = (uint64_t *)((pdpt[pdpt_idx] & ~0xFFF & ~PTE_NX) + hhdm_offset);

    if (!(pd[pd_idx] & PTE_PRESENT)) return false;
    if (pd[pd_idx] & PTE_HUGE) return true; // 2MB page
    uint64_t *pt = (uint64_t *)((pd[pd_idx] & ~0xFFF & ~PTE_NX) + hhdm_offset);

    return (pt[pt_idx] & PTE_PRESENT) != 0;
}

uint64_t vmm_create_pml4(void) {
    uint64_t phys = pmm_alloc();
    if (!phys) return 0;
    
    uint64_t *pml4 = (uint64_t *)(phys + hhdm_offset);
    memset(pml4, 0, PAGE_SIZE);
    
    // Copy kernel mappings (upper half: indices 256 to 511)
    for (int i = 256; i < 512; i++) {
        pml4[i] = kernel_pml4[i];
    }
    
    return phys;
}

void vmm_destroy_pml4(uint64_t cr3) {
    if (!cr3) return;
    
    uint64_t *pml4 = (uint64_t *)(cr3 + hhdm_offset);
    
    // Only destroy the lower half (user space, indices 0-255)
    for (int i = 0; i < 256; i++) {
        if ((pml4[i] & PTE_PRESENT) == 0) continue;
        uint64_t *pdpt = (uint64_t *)((pml4[i] & ~0xFFF & ~PTE_NX) + hhdm_offset);
        
        for (int j = 0; j < 512; j++) {
            if ((pdpt[j] & PTE_PRESENT) == 0) continue;
            if (pdpt[j] & PTE_HUGE) {
                pmm_free(pdpt[j] & ~0xFFF & ~PTE_NX);
                continue;
            }
            uint64_t *pd = (uint64_t *)((pdpt[j] & ~0xFFF & ~PTE_NX) + hhdm_offset);
            
            for (int k = 0; k < 512; k++) {
                if ((pd[k] & PTE_PRESENT) == 0) continue;
                if (pd[k] & PTE_HUGE) {
                    pmm_free(pd[k] & ~0xFFF & ~PTE_NX);
                    continue;
                }
                uint64_t *pt = (uint64_t *)((pd[k] & ~0xFFF & ~PTE_NX) + hhdm_offset);
                
                for (int l = 0; l < 512; l++) {
                    if ((pt[l] & PTE_PRESENT) == 0) continue;
                    uint64_t phys = pt[l] & ~0xFFF & ~PTE_NX;
                    pmm_free(phys);
                }
                pmm_free(pd[k] & ~0xFFF & ~PTE_NX);
            }
            pmm_free(pdpt[j] & ~0xFFF & ~PTE_NX);
        }
        pmm_free(pml4[i] & ~0xFFF & ~PTE_NX);
    }
    
    // Free the PML4 table itself
    pmm_free(cr3);
}
