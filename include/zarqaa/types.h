/*
 * Zarqaa OS 0.1 - Type Definitions
 * Creator: Abdelhak Chabbi
 *
 * Fundamental type definitions for the Zarqaa OS kernel.
 * This file provides standard integer types, size types, and
 * boolean support for a freestanding C environment.
 *
 * In a freestanding environment, GCC provides:
 *   stdint.h, stddef.h, stdbool.h, stdarg.h, float.h, limits.h
 * We re-export these and add OS-specific types.
 */

#ifndef ZARQAA_TYPES_H
#define ZARQAA_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifndef SHRT_MAX
#define SHRT_MAX 32767
#endif

#ifndef SHRT_MIN
#define SHRT_MIN (-SHRT_MAX - 1)
#endif

#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

#ifndef INT_MIN
#define INT_MIN (-INT_MAX - 1)
#endif

#ifndef UINT_MAX
#define UINT_MAX 4294967295U
#endif

#ifndef SIZE_MAX
#define SIZE_MAX (~(size_t)0)
#endif

#ifndef LONG_MAX
#define LONG_MAX 9223372036854775807L
#endif

#ifndef LONG_MIN
#define LONG_MIN (-LONG_MAX - 1L)
#endif

#ifndef ULONG_MAX
#define ULONG_MAX 18446744073709551615UL
#endif

/* ===== Basic Type Aliases ===== */

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;

typedef size_t    usize;
typedef int64_t   ssize_t;

/* ===== Color Type (32-bit ARGB) ===== */

typedef uint32_t color_t;

/* Pack RGB values into a 32-bit color (0xAARRGGBB format) */
static inline color_t color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Pack ARGB values into a 32-bit color */
static inline color_t color_argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Extract color components */
static inline uint8_t color_a(color_t c) { return (c >> 24) & 0xFF; }
static inline uint8_t color_r(color_t c) { return (c >> 16) & 0xFF; }
static inline uint8_t color_g(color_t c) { return (c >>  8) & 0xFF; }
static inline uint8_t color_b(color_t c) { return  c        & 0xFF; }

/* ===== Zarqaa OS Color Scheme (Dark Theme) ===== */

#define COLOR_BG_DARK        0xFF06111D   /* Deep dark background */
#define COLOR_BG_GRADIENT    0xFF09192A   /* Gradient end */
#define COLOR_TOPBAR         0xFF0A1828   /* Top bar surface */
#define COLOR_DOCK           0xFF0D2034   /* Dock surface */
#define COLOR_WINDOW_TITLE   0xFF0E2135   /* Window titlebar */
#define COLOR_WINDOW_BODY    0xFF081422   /* Window body */
#define COLOR_WINDOW_BORDER  0xFF36BAFF   /* Accent cyan border */
#define COLOR_TEXT_PRIMARY   0xFFE8F6FF   /* Light text */
#define COLOR_TEXT_SECONDARY 0xFF87A3BC   /* Muted text */
#define COLOR_ACCENT         0xFF36BAFF   /* Cyan accent */
#define COLOR_ACCENT2        0xFF8DDFFF   /* Light cyan accent */
#define COLOR_SUCCESS        0xFF48D99D   /* Green */
#define COLOR_WARNING        0xFFFFCB6B   /* Amber */
#define COLOR_ERROR          0xFFFF7289   /* Red */
#define COLOR_BUTTON_HOVER   0xFF195D90   /* Cyan/Blue hover */
#define COLOR_TEXT_LIGHT   0xFFE0E0E0
#define COLOR_TEXT_DARK    0xFF1A1A1A
#define COLOR_BLACK        0xFF000000
#define COLOR_WHITE        0xFFFFFFFF
#define COLOR_TRANSPARENT  0x00000000

/* ===== Geometry Types ===== */

typedef struct {
    int32_t x;
    int32_t y;
} point_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} rect_t;

/* Check if a point is inside a rectangle */
static inline bool rect_contains(const rect_t *r, int32_t px, int32_t py) {
    return px >= r->x && px < r->x + r->width &&
           py >= r->y && py < r->y + r->height;
}

/* Check if two rectangles overlap */
static inline bool rect_intersects(const rect_t *a, const rect_t *b) {
    return a->x < b->x + b->width  && a->x + a->width  > b->x &&
           a->y < b->y + b->height && a->y + a->height > b->y;
}

/* ===== I/O Port Access ===== */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = val & 0xFFFFFFFF;
    uint32_t high = val >> 32;
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Brief I/O wait (write to unused port 0x80) */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* ===== CPU Control ===== */

static inline void cli(void) {
    __asm__ volatile ("cli");
}

static inline void sti(void) {
    __asm__ volatile ("sti");
}

static inline uint64_t save_irq_disable(void) {
    uint64_t rflags;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(rflags) : : "memory");
    return rflags;
}

static inline void restore_irq(uint64_t rflags) {
    __asm__ volatile ("push %0; popfq" : : "r"(rflags) : "memory");
}

static inline void hlt(void) {
    __asm__ volatile ("hlt");
}

/* Halt forever (used for unrecoverable errors) */
static inline void hang(void) {
    cli();
    for (;;) hlt();
}

/* ===== Compiler Hints ===== */

#define UNUSED(x)       ((void)(x))
#define PACKED          __attribute__((packed))
#define ALIGNED(n)      __attribute__((aligned(n)))
#define SECTION(s)      __attribute__((section(s)))
#define NORETURN        __attribute__((noreturn))
#define ALWAYS_INLINE   __attribute__((always_inline)) inline

/* Kernel panic - declared here, defined in kernel/main.c */
void kprintf(const char *fmt, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *fmt, __builtin_va_list args);
void panic(const char *msg);
int klog_read(char *out_buf, int max_len);

#endif /* ZARQAA_TYPES_H */
