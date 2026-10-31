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

/** Number of APs ready to take parallel jobs (0 on a single-CPU system). */
int smp_worker_count(void);

/**
 * @brief Run fn(arg) on the caller and every ready AP, and wait for all.
 *
 * fn must divide the work itself (e.g. by claiming chunks atomically) and
 * must not allocate memory. Falls back to a plain call without workers.
 */
void smp_parallel_run(void (*fn)(void *), void *arg);

#endif
