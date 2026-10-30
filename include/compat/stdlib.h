/* Freestanding stub for stdlib.h - maps to Zarqaa OS kernel functions */
#ifndef _STDLIB_H_STUB
#define _STDLIB_H_STUB

#include "zarqaa/types.h"
#include "kernel/heap.h"

#define malloc  kmalloc
#define calloc  kcalloc
#define realloc krealloc
#define free    kfree

static inline int abs(int x) { return x < 0 ? -x : x; }
static inline double fabs(double x) { return x < 0.0 ? -x : x; }
static inline void exit(int status) { (void)status; while(1); }

int rand(void);
void srand(unsigned int seed);

/* strtol stub - minimal implementation for stb_image */
static inline long strtol(const char *s, char **endp, int base) {
    (void)base;
    long val = 0;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
    if (endp) *endp = (char *)s;
    return neg ? -val : val;
}

#endif
