/* Freestanding stub for stdio.h */
#ifndef _STDIO_H_STUB
#define _STDIO_H_STUB

/* stb_image doesn't actually need stdio when STBI_NO_STDIO is defined,
   but this stub prevents errors if it's conditionally included */

#ifndef FILENAME_MAX
#define FILENAME_MAX 256
#endif

extern void kprintf(const char *fmt, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *fmt, __builtin_va_list args);

static inline int printf(const char *fmt, ...) {
    char buf[256];
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    __builtin_va_end(args);
    kprintf("%s", buf);
    return 0;
}

#define fprintf(stream, ...) printf(__VA_ARGS__)

#endif
