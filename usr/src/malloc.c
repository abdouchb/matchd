#include "stdlib.h"
#include "zarqaa.h"

// A very simple bump allocator for User Space that requests memory using SYS_MMAP

static uint8_t *heap_ptr = 0;
static size_t heap_remaining = 0;

void* malloc(size_t size) {
    if (size == 0) return NULL;
    
    // Align to 8 bytes
    size = (size + 7) & ~7;
    
    if (heap_remaining < size) {
        // Request more memory from kernel via SYS_MMAP
        // 0 = let kernel pick addr, 4096 = page size, 7 = prot (user | rw | present)
        size_t alloc_size = size < 4096 ? 4096 : ((size / 4096) + 1) * 4096;
        uint64_t addr = syscall3(SYS_MMAP, 0, alloc_size, 7);
        if (addr == (uint64_t)-1) {
            return NULL; // OOM
        }
        heap_ptr = (uint8_t*)addr;
        heap_remaining = alloc_size;
    }
    
    void *ret = heap_ptr;
    heap_ptr += size;
    heap_remaining -= size;
    return ret;
}

void free(void* ptr) {
    // Basic bump allocator doesn't free
    (void)ptr;
}

void* calloc(size_t num, size_t size) {
    size_t total = num * size;
    void *ptr = malloc(total);
    if (ptr) {
        uint8_t *p = (uint8_t*)ptr;
        for (size_t i = 0; i < total; i++) {
            p[i] = 0;
        }
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    if (!ptr) {
        return malloc(size);
    }
    // We don't know the old size, just allocate a new block and let user handle copying
    // (A real realloc needs a proper heap block header)
    return malloc(size);
}
