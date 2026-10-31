/*
 * Zarqaa OS 0.1 - Kernel Heap Allocator Header
 * Creator: Abdelhak Chabbi
 */

#ifndef KERNEL_HEAP_H
#define KERNEL_HEAP_H

#include "zarqaa/types.h"

void heap_init(uint64_t start_virt, size_t size);
void *kmalloc(size_t size);
void kfree(void *ptr);
void *krealloc(void *ptr, size_t new_size);
void *kcalloc(size_t count, size_t size);
size_t heap_get_total(void);
size_t heap_get_used(void);

#endif /* KERNEL_HEAP_H */
