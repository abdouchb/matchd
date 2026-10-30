/*
 * Zarqaa OS 0.1 - POSIX Syscall Wrappers (Userland LibC stub)
 * Creator: Abdelhak Chabbi
 */

#include "unistd.h"
#include "fcntl.h"
#include "kernel/syscall.h" // For SYS_ constants

int open(const char *pathname, int flags, ...) {
    (void)flags; // Currently ignored by our simple VFS
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_OPEN), "D"(pathname), "S"(flags)
        : "rcx", "r11", "memory"
    );
    return (int)ret;
}

ssize_t read(int fd, void *buf, size_t count) {
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return (ssize_t)ret;
}

ssize_t write(int fd, const void *buf, size_t count) {
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return (ssize_t)ret;
}

int close(int fd) {
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLOSE), "D"(fd)
        : "rcx", "r11", "memory"
    );
    return (int)ret;
}
