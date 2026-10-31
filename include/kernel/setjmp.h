#pragma once

#include <stdint.h>

/* x86_64 setjmp buffer:
 * Need to save callee-saved registers:
 * RBX, RBP, R12, R13, R14, R15, RSP, and RIP
 * Total: 8 registers * 8 bytes = 64 bytes
 */
typedef struct {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rip;
} jmp_buf[1];

// Declarations (implemented in assembly)
int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);
