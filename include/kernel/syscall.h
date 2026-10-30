#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include "kernel/idt.h"

#define SYS_EXIT   0
#define SYS_READ   1
#define SYS_WRITE  2
#define SYS_OPEN   3
#define SYS_CLOSE  4
#define SYS_SOCKET 5
#define SYS_CONNECT 6
#define SYS_SEND   7
#define SYS_RECV   8
#define SYS_EXEC   9
#define SYS_YIELD  10
#define SYS_MMAP   11

void syscall_dispatcher(struct interrupt_frame *frame);

#endif
