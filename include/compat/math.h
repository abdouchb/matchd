/* Freestanding stub for math.h */
#ifndef _MATH_H_STUB
#define _MATH_H_STUB

#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))

#define signbit(x) __builtin_signbit(x)
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)

#define floorf(x) ((float)floor((double)(x)))
#define ceilf(x)  ((float)ceil((double)(x)))
#define truncf(x) ((float)trunc((double)(x)))
#define fabsf(x)  ((float)fabs((double)(x)))

static inline double fabs(double x) {
    return x < 0.0 ? -x : x;
}

static inline double ceil(double x) {
    long long i = (long long)x;
    if (x == (double)i) return x;
    if (x > 0.0) return (double)(i + 1);
    return (double)i;
}

static inline double floor(double x) {
    long long i = (long long)x;
    if (x == (double)i) return x;
    if (x < 0.0) return (double)(i - 1);
    return (double)i;
}

static inline double trunc(double x) {
    return (double)((long long)x);
}

static inline double round(double x) {
    if (x >= 0.0) return floor(x + 0.5);
    return ceil(x - 0.5);
}

static inline double pow(double base, double exp) {
    double result = 1.0;
    int e = (int)exp;
    for (int i = 0; i < e; i++) result *= base;
    return result;
}

static inline double ldexp(double x, int exp) {
    while (exp > 0) { x *= 2.0; exp--; }
    while (exp < 0) { x *= 0.5; exp++; }
    return x;
}

#endif
