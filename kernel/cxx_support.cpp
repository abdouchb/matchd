#include <stddef.h>
#include <stdint.h>

extern "C" {
    void* kmalloc(size_t size);
    void kfree(void* ptr);
    void kprintf(const char* fmt, ...);
}

// Global operators for new and delete
void* operator new(size_t size) {
    return kmalloc(size);
}

void* operator new[](size_t size) {
    return kmalloc(size);
}

void operator delete(void* p) noexcept {
    kfree(p);
}

void operator delete[](void* p) noexcept {
    kfree(p);
}

void operator delete(void* p, size_t) noexcept {
    kfree(p);
}

void operator delete[](void* p, size_t) noexcept {
    kfree(p);
}

// Pure virtual function handler
extern "C" void __cxa_pure_virtual() {
    kprintf("PANIC: Pure virtual function called!\n");
    while (1) {
        asm volatile("hlt");
    }
}

// Global constructors
typedef void (*constructor_t)();
extern "C" constructor_t __init_array_start[];
extern "C" constructor_t __init_array_end[];

extern "C" void kernel_init_cxx() {
    for (constructor_t* i = __init_array_start; i < __init_array_end; ++i) {
        (*i)();
    }
}

// C++ ABI for static variables and global destructors
extern "C" {
    int __cxa_guard_acquire(uint64_t* guard_object) {
        // Return 1 if initialization is required
        if (*guard_object == 0) return 1;
        return 0;
    }

    void __cxa_guard_release(uint64_t* guard_object) {
        *guard_object = 1;
    }

    void __cxa_guard_abort(uint64_t* guard_object) {
        (void)guard_object;
    }

    int __cxa_atexit(void (*destructor)(void*), void* arg, void* dso_handle) {
        // We don't destruct objects on OS shutdown
        (void)destructor;
        (void)arg;
        (void)dso_handle;
        return 0;
    }

    void* __dso_handle = nullptr;
}
