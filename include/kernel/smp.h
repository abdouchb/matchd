/*
 * Zarqaa OS - Symmetric Multiprocessing (SMP) Initialization
 * 
 * Phase 1: Bring all Application Processors (APs) online.
 */

#ifndef SMP_H
#define SMP_H

#include <stdint.h>

/**
 * @brief Initialize Symmetric Multiprocessing (SMP)
 * 
 * Parses the Limine SMP response, allocates per-CPU stacks for the
 * Application Processors (APs), and starts them by assigning their
 * entry point.
 */
void smp_init(void);

#endif
