/*
 * Zarqaa OS 0.1 - Printf Implementation
 * Creator: Abdelhak Chabbi
 */

#include "zarqaa/types.h"
#include "zarqaa/string.h"
#include "drivers/serial.h"
#include <stdarg.h>

#define KLOG_SIZE 8192
static char klog_buf[KLOG_SIZE];
static int klog_head = 0;
static int klog_tail = 0;

static void kprintf_write_char(char c) {
    serial_write_char(c);
    klog_buf[klog_head] = c;
    klog_head = (klog_head + 1) % KLOG_SIZE;
    if (klog_head == klog_tail) {
        klog_tail = (klog_tail + 1) % KLOG_SIZE;
    }
}


int klog_read(char *out_buf, int max_len) {
    int count = 0;
    int cur = klog_tail;
    while (cur != klog_head && count < max_len - 1) {
        out_buf[count++] = klog_buf[cur];
        cur = (cur + 1) % KLOG_SIZE;
    }
    out_buf[count] = '\0';
    return count;
}

typedef void (*putc_func_t)(char c, void *ctx);

struct vsnprintf_ctx {
    char *str;
    size_t size;
    int count;
};

static void vsnprintf_putc(char c, void *ctx_p) {
    struct vsnprintf_ctx *ctx = (struct vsnprintf_ctx *)ctx_p;
    if (ctx->str && ctx->size > 0 && ctx->count < (int)(ctx->size - 1)) {
        ctx->str[ctx->count] = c;
    }
    ctx->count++;
}

static void kprintf_putc(char c, void *ctx) {
    (void)ctx;
    kprintf_write_char(c);
}

static int format_string(putc_func_t putc, void *ctx, const char *fmt, va_list args) {
    int count = 0;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == '%') {
                putc('%', ctx);
                count++;
                fmt++;
                continue;
            }

            int pad = 0;
            char pad_char = ' ';
            if (*fmt == '0') {
                pad_char = '0';
                fmt++;
            }
            while (*fmt >= '0' && *fmt <= '9') {
                pad = pad * 10 + (*fmt - '0');
                fmt++;
            }

            int is_long = 0;
            int is_long_long = 0;
            if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'l') {
                    is_long_long = 1;
                    fmt++;
                } else {
                    is_long = 1;
                }
            } else if (*fmt == 'z') {
                is_long_long = 1;
                fmt++;
            }

            switch (*fmt) {
                case 'd':
                case 'i': {
                    long long val;
                    if (is_long_long) val = va_arg(args, long long);
                    else if (is_long) val = va_arg(args, long);
                    else val = va_arg(args, int);
                    
                    char buf[64];
                    itoa64(val, buf, 10);
                    int len = 0;
                    while (buf[len]) len++;
                    while (len < pad) { putc(pad_char, ctx); count++; pad--; }
                    for (int i = 0; buf[i]; i++) { putc(buf[i], ctx); count++; }
                    break;
                }
                case 'u': {
                    unsigned long long val;
                    if (is_long_long) val = va_arg(args, unsigned long long);
                    else if (is_long) val = va_arg(args, unsigned long);
                    else val = va_arg(args, unsigned int);
                    
                    char buf[64];
                    utoa64(val, buf, 10);
                    int len = 0;
                    while (buf[len]) len++;
                    while (len < pad) { putc(pad_char, ctx); count++; pad--; }
                    for (int i = 0; buf[i]; i++) { putc(buf[i], ctx); count++; }
                    break;
                }
                case 'x':
                case 'X': {
                    unsigned long long val;
                    if (is_long_long) val = va_arg(args, unsigned long long);
                    else if (is_long) val = va_arg(args, unsigned long);
                    else val = va_arg(args, unsigned int);
                    
                    char buf[64];
                    utoa64(val, buf, 16);
                    int len = 0;
                    while (buf[len]) len++;
                    while (len < pad) { putc(pad_char, ctx); count++; pad--; }
                    for (int i = 0; buf[i]; i++) {
                        char c = buf[i];
                        if (*fmt == 'X' && c >= 'a' && c <= 'f') c -= 32;
                        putc(c, ctx);
                        count++;
                    }
                    break;
                }
                case 'p': {
                    void *ptr = va_arg(args, void *);
                    char buf[64];
                    putc('0', ctx); count++;
                    putc('x', ctx); count++;
                    utoa64((unsigned long long)(uint64_t)ptr, buf, 16);
                    int len = 0;
                    while (buf[len]) len++;
                    while (len < pad) { putc(pad_char, ctx); count++; pad--; }
                    for (int i = 0; buf[i]; i++) { putc(buf[i], ctx); count++; }
                    break;
                }
                case 's': {
                    const char *s = va_arg(args, const char *);
                    if (!s) s = "(null)";
                    int len = 0;
                    while (s[len]) len++;
                    while (len < pad) { putc(' ', ctx); count++; pad--; }
                    for (int i = 0; s[i]; i++) { putc(s[i], ctx); count++; }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    while (1 < pad) { putc(' ', ctx); count++; pad--; }
                    putc(c, ctx);
                    count++;
                    break;
                }
                default:
                    putc('%', ctx);
                    count++;
                    if (*fmt) {
                        putc(*fmt, ctx);
                        count++;
                    } else {
                        fmt--;
                    }
                    break;
            }
        } else {
            putc(*fmt, ctx);
            count++;
        }
        fmt++;
    }
    return count;
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    format_string(kprintf_putc, NULL, fmt, args);
    va_end(args);
}

void panic(const char *msg) {
    kprintf("\n================================================\n");
    kprintf("KERNEL PANIC: %s\n", msg);
    kprintf("================================================\n");
    hang();
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list args) {
    struct vsnprintf_ctx ctx = {str, size, 0};
    int count = format_string(vsnprintf_putc, &ctx, fmt, args);
    if (str && size > 0) {
        if (count < (int)size) {
            str[count] = '\0';
        } else {
            str[size - 1] = '\0';
        }
    }
    return count;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}

int sprintf(char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    /* Pass a very large size since sprintf doesn't check bounds */
    int ret = vsnprintf(str, 0x7FFFFFFF, format, args);
    va_end(args);
    return ret;
}
