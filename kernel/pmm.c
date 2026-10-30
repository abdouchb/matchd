/*
 * Zarqaa OS 0.1 - Physical Memory Manager
 * Creator: Abdelhak Chabbi
 */

#include "kernel/pmm.h"
#include "kernel/spinlock.h"
#include "zarqaa/string.h"

static spinlock_t pmm_lock = SPINLOCK_INIT;
static uint8_t *bitmap = NULL;
static uint64_t highest_page = 0;
static uint64_t last_alloc_index = 0;
static uint64_t total_memory = 0;
static uint64_t used_memory = 0;

static inline void bitmap_set(uint64_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(uint64_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline bool bitmap_test(uint64_t bit) {
    return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset) {
    uint64_t highest_addr = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            total_memory += entry->length;
        }
        if (entry->base + entry->length > highest_addr) {
            highest_addr = entry->base + entry->length;
        }
    }

    highest_page = highest_addr / PAGE_SIZE;
    /* Round up bytes needed for bitmap */
    uint64_t bitmap_size = (highest_page + 7) / 8;
    if (bitmap_size == 0) bitmap_size = 1;

    /* Determine pages required for bitmap and find a usable, page-aligned region */
    uint64_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_pages * PAGE_SIZE + PAGE_SIZE) {
            uint64_t aligned_base = (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            if (aligned_base + bitmap_pages * PAGE_SIZE <= entry->base + entry->length) {
                bitmap = (uint8_t *)(aligned_base + hhdm_offset);
                break;
            }
        }
    }

    if (!bitmap) {
        /* Fallback: try first usable entry without strict alignment */
        for (uint64_t i = 0; i < memmap->entry_count; i++) {
            struct limine_memmap_entry *entry = memmap->entries[i];
            if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
                bitmap = (uint8_t *)(entry->base + hhdm_offset);
                break;
            }
        }
    }

    if (!bitmap) {
        panic("PMM: Failed to find memory for bitmap!\n");
    }

    /* Initialize all memory to USED */
    memset(bitmap, 0xFF, bitmap_size);

    /* Start with all usable memory as "used" (since bitmap is all 1s) */
    used_memory = total_memory;

    /* Free the usable regions (clear bits, subtract from used) */
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            for (uint64_t j = 0; j < entry->length; j += PAGE_SIZE) {
                bitmap_clear((entry->base + j) / PAGE_SIZE);
            }
        }
    }

    /* Now all usable pages are free. used = only the bitmap overhead */
    uint64_t bitmap_phys = (uint64_t)bitmap - hhdm_offset;
    bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    used_memory = bitmap_pages * PAGE_SIZE;

    for (uint64_t i = 0; i < bitmap_pages; i++) {
        bitmap_set((bitmap_phys / PAGE_SIZE) + i);
    }
    
    /* Permanently reserve page 0 to avoid returning NULL physical addresses */
    if (!bitmap_test(0)) {
        bitmap_set(0);
        used_memory += PAGE_SIZE;
    }
}

uint64_t pmm_alloc(void) {
    spin_lock(&pmm_lock);
    for (uint64_t i = last_alloc_index; i < highest_page; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_memory += PAGE_SIZE;
            last_alloc_index = i;
            spin_unlock(&pmm_lock);
            return i * PAGE_SIZE;
        }
    }

    for (uint64_t i = 0; i < last_alloc_index; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_memory += PAGE_SIZE;
            last_alloc_index = i;
            spin_unlock(&pmm_lock);
            return i * PAGE_SIZE;
        }
    }

    spin_unlock(&pmm_lock);
    return 0; // Out of memory
}

uint64_t pmm_alloc_blocks(uint64_t count) {
    if (count == 0) return 0;
    
    spin_lock(&pmm_lock);
    if (count > highest_page) {
        spin_unlock(&pmm_lock);
        return 0;
    }
    uint64_t limit = highest_page - count;

    for (uint64_t i = last_alloc_index; i <= limit; i++) {
        bool free = true;
        for (uint64_t j = 0; j < count; j++) {
            if (bitmap_test(i + j)) {
                free = false;
                i += j; /* Skip checked blocks */
                break;
            }
        }
        
        if (free) {
            for (uint64_t j = 0; j < count; j++) {
                bitmap_set(i + j);
            }
            used_memory += count * PAGE_SIZE;
            last_alloc_index = i + count;
            spin_unlock(&pmm_lock);
            return i * PAGE_SIZE;
        }
    }

    /* Wrap-around scan */
    uint64_t wrap_limit = (last_alloc_index < limit) ? last_alloc_index : limit;
    for (uint64_t i = 1; i < wrap_limit; i++) {
        bool free = true;
        for (uint64_t j = 0; j < count; j++) {
            if (bitmap_test(i + j)) {
                free = false;
                i += j;
                break;
            }
        }
        
        if (free) {
            for (uint64_t j = 0; j < count; j++) {
                bitmap_set(i + j);
            }
            used_memory += count * PAGE_SIZE;
            last_alloc_index = i + count;
            spin_unlock(&pmm_lock);
            return i * PAGE_SIZE;
        }
    }
    
    spin_unlock(&pmm_lock);
    return 0; /* Out of contiguous memory */
}

void pmm_free(uint64_t addr) {
    uint64_t bit = addr / PAGE_SIZE;
    spin_lock(&pmm_lock);
    if (bit < highest_page && bitmap_test(bit)) {
        bitmap_clear(bit);
        used_memory -= PAGE_SIZE;
        if (bit < last_alloc_index) {
            last_alloc_index = bit;
        }
    }
    spin_unlock(&pmm_lock);
}

uint64_t pmm_get_total(void) { return total_memory; }
uint64_t pmm_get_used(void)  { return used_memory; }
uint64_t pmm_get_free(void)  { return total_memory - used_memory; }

/* Allocate a physical page below 4GB (for 32-bit DMA controllers) */
uint64_t pmm_alloc_low32(void) {
    uint64_t max_page = 0x100000000ULL / PAGE_SIZE; /* page index for 4GB */
    if (max_page > highest_page) max_page = highest_page;
    
    spin_lock(&pmm_lock);
    for (uint64_t i = 1; i < max_page; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_memory += PAGE_SIZE;
            spin_unlock(&pmm_lock);
            return i * PAGE_SIZE;
        }
    }
    spin_unlock(&pmm_lock);
    return 0; /* No low memory available */
}

/* Allocate multiple contiguous pages below 4GB */
uint64_t pmm_alloc_blocks_low32(uint64_t count) {
    if (count == 0) return 0;
    
    uint64_t max_page = 0x100000000ULL / PAGE_SIZE;
    if (max_page > highest_page) max_page = highest_page;
    
    spin_lock(&pmm_lock);
    if (count >= max_page) {
        spin_unlock(&pmm_lock);
        return 0;
    }
    
    uint64_t limit = max_page - count;
    for (uint64_t i = 1; i <= limit; i++) {
        bool free = true;
        for (uint64_t j = 0; j < count; j++) {
            if (bitmap_test(i + j)) {
                free = false;
                i += j;
                break;
            }
        }
        
        if (free) {
            for (uint64_t j = 0; j < count; j++) {
                bitmap_set(i + j);
            }
            used_memory += count * PAGE_SIZE;
            spin_unlock(&pmm_lock);
            return i * PAGE_SIZE;
        }
    }
    spin_unlock(&pmm_lock);
    return 0;
}

