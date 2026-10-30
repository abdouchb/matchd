#ifndef ZARQAA_USR_H
#define ZARQAA_USR_H

#include <stddef.h>
#include <stdint.h>

#define SYS_EXIT  1
#define SYS_OPEN  2
#define SYS_READ  3
#define SYS_WRITE 4
#define SYS_CLOSE 5
#define SYS_ACCEPT 6
#define SYS_BIND   7
#define SYS_RECV   8
#define SYS_EXEC   9
#define SYS_YIELD  10
#define SYS_MMAP   11
#define SYS_POLLKEY 12
#define SYS_SLEEP   13

static inline uint64_t syscall0(uint64_t num) {
    uint64_t ret;
    __asm__ volatile("int $128" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory");
    return ret;
}

static inline uint64_t syscall1(uint64_t num, uint64_t arg1) {
    uint64_t ret;
    __asm__ volatile("int $128" : "=a"(ret) : "a"(num), "D"(arg1) : "rcx", "r11", "memory");
    return ret;
}

static inline uint64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    uint64_t ret;
    __asm__ volatile("int $128" : "=a"(ret) : "a"(num), "D"(arg1), "S"(arg2) : "rcx", "r11", "memory");
    return ret;
}

static inline uint64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t ret;
    __asm__ volatile("int $128" : "=a"(ret) : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3) : "rcx", "r11", "memory");
    return ret;
}

static inline void exit(void) {
    syscall0(SYS_EXIT);
    while (1);
}

static inline int write(int fd, const void *buf, size_t count) {
    return (int)syscall3(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

static inline int read(int fd, void *buf, size_t count) {
    return (int)syscall3(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

static inline int pollkey(void) {
    return (int)syscall0(SYS_POLLKEY);
}

static inline void sleep_ms(uint64_t ms) {
    syscall1(SYS_SLEEP, ms);
}

static inline void yield(void) {
    syscall0(SYS_YIELD);
}

#endif
