#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    volatile uint32_t lock;
} spinlock_t;

static inline void spin_lock(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->lock, 1)) {
        while (lock->lock) {
            __asm__ volatile("pause");
        }
    }
}

static inline void spin_unlock(spinlock_t *lock) {
    __sync_lock_release(&lock->lock);
}

#define SPINLOCK_INIT {0}

#endif
