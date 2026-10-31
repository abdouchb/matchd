#include "zarqaa/types.h"

// stb_image uses a few soft-float helpers (normally provided by libgcc).
// We compile the kernel with -mno-sse2, but we can re-enable SSE2 for
// individual functions using __attribute__((target(...))) so they use
// hardware FP instructions instead of recursively calling themselves.

__attribute__((target("sse,sse2")))
float __divsf3(float a, float b) {
    return a / b;
}

__attribute__((target("sse,sse2")))
int __nesf2(float a, float b) {
    return a != b;
}

__attribute__((target("sse,sse2")))
int __eqsf2(float a, float b) {
    return a == b;
}

__attribute__((target("sse,sse2")))
int __ltsf2(float a, float b) {
    return a < b;
}

__attribute__((target("sse,sse2")))
float __mulsf3(float a, float b) {
    return a * b;
}

__attribute__((target("sse,sse2")))
float __addsf3(float a, float b) {
    return a + b;
}

__attribute__((target("sse,sse2")))
float __subsf3(float a, float b) {
    return a - b;
}

__attribute__((target("sse,sse2")))
int __fixsfsi(float a) {
    return (int)a;
}

__attribute__((target("sse,sse2")))
float __floatsisf(int a) {
    return (float)a;
}

__attribute__((target("sse,sse2")))
unsigned int __fixunssfsi(float a) {
    return (unsigned int)a;
}

__attribute__((target("sse,sse2")))
int __gtsf2(float a, float b) {
    return a > b;
}

__attribute__((target("sse,sse2")))
int __gesf2(float a, float b) {
    return a >= b;
}

__attribute__((target("sse,sse2")))
int __lesf2(float a, float b) {
    return a <= b;
}
