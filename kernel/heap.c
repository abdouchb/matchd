/*
 * Zarqaa OS 0.1 - Kernel Heap Allocator
 * Creator: Abdelhak Chabbi
 */

#include "kernel/heap.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "zarqaa/string.h"
#include "drivers/serial.h"
#include "kernel/spinlock.h"

static spinlock_t heap_lock = SPINLOCK_INIT;

struct heap_block {
    size_t size;
    bool is_free;
    struct heap_block *next;
    uint64_t magic; // 0xDEADBEEFC0DEF00D
};

static struct heap_block *heap_head = NULL;
static uint64_t heap_start = 0;
static size_t heap_total_size = 0;

#define ALIGN(x) (((x) + 15) & ~15)

void heap_init(uint64_t start_virt, size_t size) {
    heap_start = start_virt;
    heap_total_size = size;

    /* Pages are pre-mapped (using static initial_heap) */

    heap_head = (struct heap_block *)start_virt;
    heap_head->size = size - sizeof(struct heap_block);
    heap_head->is_free = true;
    heap_head->next = NULL;
    heap_head->magic = 0xDEADBEEFC0DEF00D;
}

static void heap_coalesce(void) {
    struct heap_block *current = heap_head;
    while (current != NULL && current->next != NULL) {
        if (current->is_free && current->next->is_free) {
            current->size += sizeof(struct heap_block) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = ALIGN(size);

    uint64_t rflags = save_irq_disable();
    spin_lock(&heap_lock);

    struct heap_block *current = heap_head;
    while (current != NULL) {
        if (current->magic != 0xDEADBEEFC0DEF00D) {
            spin_unlock(&heap_lock);
            restore_irq(rflags);
            kprintf("[kmalloc] HEAP CORRUPTION DETECTED at block %p\n", current);
            panic("Heap corruption in kmalloc");
        }
        
        if (current->is_free && current->size >= size) {
            /* Split block if enough space */
            if (current->size >= size + sizeof(struct heap_block) + 32) {
                struct heap_block *new_block = (struct heap_block *)((uint8_t *)current + sizeof(struct heap_block) + size);
                new_block->size = current->size - size - sizeof(struct heap_block);
                new_block->is_free = true;
                new_block->next = current->next;
                new_block->magic = 0xDEADBEEFC0DEF00D;

                current->size = size;
                current->next = new_block;
            }
            current->is_free = false;
            
            void *allocated_ptr = (void *)((uint8_t *)current + sizeof(struct heap_block));
            
            spin_unlock(&heap_lock);
            restore_irq(rflags);

            return allocated_ptr;
        }
        current = current->next;
    }

    // Try coalescing and search one more time if not found
    heap_coalesce();
    
    current = heap_head;
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            if (current->size >= size + sizeof(struct heap_block) + 32) {
                struct heap_block *new_block = (struct heap_block *)((uint8_t *)current + sizeof(struct heap_block) + size);
                new_block->size = current->size - size - sizeof(struct heap_block);
                new_block->is_free = true;
                new_block->next = current->next;
                new_block->magic = 0xDEADBEEFC0DEF00D;

                current->size = size;
                current->next = new_block;
            }
            current->is_free = false;
            
            void *allocated_ptr = (void *)((uint8_t *)current + sizeof(struct heap_block));
            
            spin_unlock(&heap_lock);
            restore_irq(rflags);

            return allocated_ptr;
        }
        current = current->next;
    }

    spin_unlock(&heap_lock);
    restore_irq(rflags);
    return NULL; /* OOM */
}

void kfree(void *ptr) {
    if (ptr == NULL) return;

    uint64_t rflags = save_irq_disable();
    spin_lock(&heap_lock);

    struct heap_block *block = (struct heap_block *)((uint8_t *)ptr - sizeof(struct heap_block));
    
    // Safety checks
    if ((uint64_t)block < heap_start || (uint64_t)block >= heap_start + heap_total_size) {
        spin_unlock(&heap_lock);
        restore_irq(rflags);
        kprintf("[kfree] ERROR: pointer %p outside heap range\n", ptr);
        return;
    }
    
    if (block->magic != 0xDEADBEEFC0DEF00D) {
        spin_unlock(&heap_lock);
        restore_irq(rflags);
        kprintf("[kfree] ERROR: Heap corruption detected! Invalid magic for pointer %p\n", ptr);
        panic("Heap corruption in kfree");
    }
    
    if (block->is_free) {
        spin_unlock(&heap_lock);
        restore_irq(rflags);
        kprintf("[kfree] WARNING: double free detected for %x\n", (uint32_t)(uint64_t)ptr);
        return;
    }
    
    block->is_free = true;

    heap_coalesce();
    
    spin_unlock(&heap_lock);
    restore_irq(rflags);
}

void *krealloc(void *ptr, size_t new_size) {
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    new_size = ALIGN(new_size);
    if (ptr == NULL) {
        return kmalloc(new_size);
    }

    uint64_t rflags = save_irq_disable();
    spin_lock(&heap_lock);
    struct heap_block *block = (struct heap_block *)((uint8_t *)ptr - sizeof(struct heap_block));
    size_t old_size = block->size;
    spin_unlock(&heap_lock);
    restore_irq(rflags);

    if (old_size >= new_size) {
        return ptr; /* already large enough */
    }

    void *new_ptr = kmalloc(new_size);
    if (new_ptr != NULL) {
        size_t copy_size = old_size < new_size ? old_size : new_size;
        memcpy(new_ptr, ptr, copy_size);
        kfree(ptr);
    }
    return new_ptr;
}

void *kcalloc(size_t count, size_t size) {
    /* Overflow guard */
    if (size != 0 && count > SIZE_MAX / size) return NULL;
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

size_t heap_get_total(void) {
    return heap_total_size;
}

size_t heap_get_used(void) {
    extern uint64_t save_irq_disable(void);
    extern void restore_irq(uint64_t rflags);
    uint64_t rflags = save_irq_disable();
    spin_lock(&heap_lock);
    size_t used = 0;
    struct heap_block *curr = heap_head;
    while (curr) {
        if (!curr->is_free) {
            used += curr->size + sizeof(struct heap_block);
        }
        curr = curr->next;
    }
    spin_unlock(&heap_lock);
    restore_irq(rflags);
    return used;
}
