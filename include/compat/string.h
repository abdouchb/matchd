/* Freestanding stub for string.h - maps to Zarqaa OS kernel functions */
#ifndef _STRING_H_STUB
#define _STRING_H_STUB

#include "zarqaa/string.h"

static inline size_t strcspn(const char *s, const char *reject) {
    size_t count = 0;
    while (*s) {
        const char *r = reject;
        while (*r) {
            if (*s == *r) return count;
            r++;
        }
        s++;
        count++;
    }
    return count;
}

#endif
