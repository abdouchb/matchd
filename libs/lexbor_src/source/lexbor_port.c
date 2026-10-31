#include <stddef.h>
#include <stdlib.h>
#include "lexbor/core/base.h"
#include "lexbor/core/lexbor.h"
#include "kernel/heap.h"

void *lexbor_malloc(size_t size) {
    return kmalloc(size);
}

void *lexbor_calloc(size_t num, size_t size) {
    return kcalloc(num, size);
}

void *lexbor_realloc(void *dst, size_t size) {
    return krealloc(dst, size);
}

void *lexbor_free(void *dst) {
    kfree(dst);
    return NULL;
}
