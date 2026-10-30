/*
 * Zarqaa OS 0.1 - Duktape Freestanding Shim v2
 * Creator: Abdelhak Chabbi
 *
 * Strategy: Define the include guards of ALL compat headers BEFORE Duktape
 * includes them. This prevents double-definition conflicts.
 * Then define ONLY what Duktape needs that our compat headers don't cover.
 */

#ifndef ZARQAA_DUK_SHIM_H
#define ZARQAA_DUK_SHIM_H

/* ── 1. Block re-inclusion of compat headers that we'll define here ─────
 * These macros must match the include guards in include/compat/*.h
 */
#define _STDLIB_H_STUB
#define _MATH_H_STUB
#define COMPAT_TIME_H
#define COMPAT_SETJMP_H

/* ── 2. Pull in zarqaa types and kernel heap (safe, no conflicts) ────── */
#include "zarqaa/types.h"
#include "kernel/heap.h"

/* ── 3. Redefine size_t correctly for this compiler ─────────────────── */
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef __SIZE_TYPE__ size_t;
#endif

#ifndef _PTRDIFF_T_DEFINED
#define _PTRDIFF_T_DEFINED
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#endif

/* ── 4. stdarg ───────────────────────────────────────────────────────── */
typedef __builtin_va_list va_list;
#ifndef va_start
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap)         __builtin_va_end(ap)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_copy(d, s)      __builtin_va_copy(d, s)
#endif

/* ── 5. Limits ───────────────────────────────────────────────────────── */
#ifndef INT_MAX
#define INT_MAX     2147483647
#define INT_MIN     (-2147483648)
#define UINT_MAX    0xFFFFFFFFU
#define LONG_MAX    9223372036854775807LL
#define LONG_MIN    ((-9223372036854775807LL) - 1)
#define ULONG_MAX   0xFFFFFFFFFFFFFFFFULL
#define CHAR_BIT    8
#define CHAR_MAX    127
#define SCHAR_MAX   127
#define ULLONG_MAX  0xFFFFFFFFFFFFFFFFULL
#endif

/* ── 6. Floating point constants ─────────────────────────────────────── */
#ifndef INFINITY
#define INFINITY  (__builtin_inff())
#endif
#ifndef NAN
#define NAN       (__builtin_nanf(""))
#endif
#ifndef HUGE_VAL
#define HUGE_VAL  (__builtin_inf())
#endif

/* FP classification - use Duktape's built-in replacements */
#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4
#define DUK_F_USE_REPL_ALL

/* FP classification macros → compiler builtins (prevents undefined symbol errors) */
#ifndef isnan
#define isnan(x)        __builtin_isnan(x)
#endif
#ifndef isfinite
#define isfinite(x)     __builtin_isfinite(x)
#endif
#ifndef isinf
#define isinf(x)        __builtin_isinf(x)
#endif
#ifndef isnormal
#define isnormal(x)     __builtin_isnormal(x)
#endif
#ifndef signbit
#define signbit(x)      __builtin_signbit(x)
#endif
#ifndef fpclassify
#define fpclassify(x)   __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, (x))
#endif


/* ── 7. String functions (from zarqaa kernel) ─────────────────────────── */
extern void  *memcpy(void *d, const void *s, size_t n);
extern void  *memmove(void *d, const void *s, size_t n);
extern void  *memset(void *s, int c, size_t n);
extern int    memcmp(const void *a, const void *b, size_t n);
extern size_t strlen(const char *s);
extern int    strcmp(const char *a, const char *b);
extern int    strncmp(const char *a, const char *b, size_t n);
extern char  *strchr(const char *s, int c);
extern char  *strncpy(char *d, const char *s, size_t n);
extern char  *strncat(char *d, const char *s, size_t n);

/* ── 8. Printf (from kernel printf.c) ────────────────────────────────── */
extern int sprintf(char *str, const char *fmt, ...);
extern int snprintf(char *str, size_t size, const char *fmt, ...);
extern int vsprintf(char *str, const char *fmt, va_list ap);
extern int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

/* sscanf - Duktape uses it only for pointer parsing debug code */
static inline int sscanf(const char *str, const char *fmt, ...)  { (void)str; (void)fmt; return 0; }
static inline int vsscanf(const char *str, const char *fmt, va_list ap) { (void)str; (void)fmt; (void)ap; return 0; }

/* ── 9. Memory allocation → kernel heap ──────────────────────────────── */
static inline void *malloc(size_t s)           { return kmalloc(s); }
static inline void  free(void *p)              { kfree(p); }
static inline void *realloc(void *p, size_t s) { return krealloc(p, s); }
static inline void *calloc(size_t n, size_t s) { return kcalloc(n, s); }

/* ── 10. abort / exit → kernel halt ─────────────────────────────────── */
static inline __attribute__((noreturn)) void abort(void) {
    __asm__ volatile("cli");
    while (1) __asm__ volatile("hlt");
}
static inline __attribute__((noreturn)) void exit(int c) { (void)c; abort(); }

/* ── 11. strtol / strtoul / strtod ──────────────────────────────────── */
static inline long strtol(const char *s, char **e, int base) {
    (void)base;
    long v = 0; int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    if (e) *e = (char *)s;
    return neg ? -v : v;
}
static inline unsigned long strtoul(const char *s, char **e, int b) { return (unsigned long)strtol(s, e, b); }
static inline double strtod(const char *s, char **e) { (void)s; (void)e; return 0.0; }
static inline int abs(int x) { return x < 0 ? -x : x; }
static inline long labs(long x) { return x < 0 ? -x : x; }
static inline int rand(void) { return 42; }
static inline void srand(unsigned int s) { (void)s; }

/* ── 12. Math functions ──────────────────────────────────────────────── */
static inline double fabs(double x)  { return x < 0.0 ? -x : x; }
static inline float  fabsf(float x)  { return x < 0.0f ? -x : x; }

static inline double floor(double x) {
    long long i = (long long)x;
    return (x < (double)i) ? (double)(i - 1) : (double)i;
}
static inline double ceil(double x) {
    long long i = (long long)x;
    return (x > (double)i) ? (double)(i + 1) : (double)i;
}
static inline double trunc(double x) { return (double)((long long)x); }
static inline double round(double x) { return (x >= 0.0) ? floor(x + 0.5) : ceil(x - 0.5); }
static inline double fmod(double x, double y) { return x - (double)(long long)(x / y) * y; }
static inline double sqrt(double x)   { return __builtin_sqrt(x); }
static inline double sin(double x)    { return __builtin_sin(x); }
static inline double cos(double x)    { return __builtin_cos(x); }
static inline double tan(double x)    { return __builtin_tan(x); }
static inline double exp(double x)    { return __builtin_exp(x); }
static inline double log(double x)    { return __builtin_log(x); }
static inline double log2(double x)   { return __builtin_log2(x); }
static inline double log10(double x)  { return __builtin_log10(x); }
static inline double asin(double x)   { return __builtin_asin(x); }
static inline double acos(double x)   { return __builtin_acos(x); }
static inline double atan(double x)   { return __builtin_atan(x); }
static inline double atan2(double y, double x) { return __builtin_atan2(y, x); }
static inline double cbrt(double x)   { return __builtin_cbrt(x); }
static inline double pow(double b, double e) {
    double r = 1.0;
    int ei = (int)e;
    for (int i = 0; i < ei; i++) r *= b;
    return r;
}
static inline double ldexp(double x, int e) {
    while (e > 0) { x *= 2.0; e--; }
    while (e < 0) { x *= 0.5; e++; }
    return x;
}
static inline double scalbn(double x, int e) { return ldexp(x, e); }
static inline double scalbln(double x, long e) { return ldexp(x, (int)e); }

/* ── 13. Time (stub - no real RTC yet) ───────────────────────────────── */
typedef long long time_t;
typedef long long clock_t;
struct tm {
    int tm_sec, tm_min, tm_hour;
    int tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
};
static inline time_t time(time_t *t) { if (t) *t = 0; return 0; }
static inline double difftime(time_t a, time_t b) { return (double)(a - b); }
static inline time_t mktime(struct tm *t) { (void)t; return 0; }
static inline struct tm *gmtime(const time_t *t) { (void)t; return (void*)0; }
static inline struct tm *localtime(const time_t *t) { (void)t; return (void*)0; }

/* ── 14. setjmp / longjmp ─────────────────────────────────────────────── */
typedef unsigned long jmp_buf[8];
extern int  setjmp(jmp_buf env);
extern void longjmp(jmp_buf env, int val);

/* ── 15. FILE / stdio stubs ──────────────────────────────────────────── */
typedef void FILE;
#define stderr ((FILE*)0)
#define stdout ((FILE*)1)
static inline int fprintf(FILE *f, const char *fmt, ...) { (void)f; (void)fmt; return 0; }
static inline int fflush(FILE *f) { (void)f; return 0; }

/* ── 16. Misc ─────────────────────────────────────────────────────────── */
#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

/* Block LLVM's stddef size_t from conflicting */
#define __DEFINED_size_t
#define _SIZE_T
#define _SIZE_T_

#endif /* ZARQAA_DUK_SHIM_H */
