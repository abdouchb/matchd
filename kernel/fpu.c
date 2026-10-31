/*
 * Zarqaa OS 0.1 - FPU / SSE Implementation
 * Creator: Abdelhak Chabbi
 */

#include "kernel/fpu.h"
#include "zarqaa/types.h"
#include "kernel/printf.h"

void fpu_init(void) {
    uint64_t cr0, cr4;

    // Read CR0
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    
    // Clear EM (bit 2) - Disable FPU emulation
    cr0 &= ~(1 << 2);
    // Set MP (bit 1) - Monitor co-processor
    cr0 |= (1 << 1);
    
    // Write back CR0
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));

    // Read CR4
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    
    // Set OSFXSR (bit 9) - OS supports FXSAVE/FXRSTOR
    cr4 |= (1 << 9);
    // Set OSXMMEXCPT (bit 10) - OS supports Unmasked SIMD FPU exceptions
    cr4 |= (1 << 10);
    
    // Write back CR4
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));

    // Initialize FPU
    __asm__ volatile ("fninit");

    kprintf("[FPU] Initialization complete (SSE enabled, FXSAVE supported).\n");
}
