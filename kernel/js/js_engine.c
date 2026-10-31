/*
 * Zarqaa OS 0.1 - JavaScript Engine Wrapper (Duktape)
 * Creator: Abdelhak Chabbi
 */

/* IMPORTANT: shim must be included BEFORE duktape.h to prevent
 * conflicts with include/compat headers that duk_config.h pulls in */
#include "zarqaa_duk_shim.h"
#include "duktape.h"
#include "zarqaa/string.h"
#include "kernel/syslog.h"

static void *duk_alloc_wrap(void *udata, duk_size_t size) {
    (void)udata;
    return kmalloc(size);
}

static void *duk_realloc_wrap(void *udata, void *ptr, duk_size_t size) {
    (void)udata;
    return krealloc(ptr, size); // Need to make sure krealloc exists in heap.h
}

static void duk_free_wrap(void *udata, void *ptr) {
    (void)udata;
    kfree(ptr);
}

static void duk_fatal_wrap(void *udata, const char *msg) {
    (void)udata;
    kprintf("Duktape Fatal Error: %s\n", msg);
    while (1) __asm__ volatile("hlt");
}

/* 
 * Print wrapper for console.log 
 */
static duk_ret_t native_print(duk_context *ctx) {
    duk_push_string(ctx, " ");
    duk_insert(ctx, 0);
    duk_join(ctx, duk_get_top(ctx) - 1);
    const char *str = duk_safe_to_string(ctx, -1);
    kprintf("[JS] %s\n", str);
    return 0;
}

duk_context *js_init(void) {
    kprintf("[JS] Initializing Duktape Engine...\n");
    // Use our custom OS heap allocator
    duk_context *ctx = duk_create_heap(duk_alloc_wrap, duk_realloc_wrap, duk_free_wrap, NULL, duk_fatal_wrap);
    
    if (ctx) {
        // Register console.log
        duk_push_c_function(ctx, native_print, DUK_VARARGS);
        duk_put_global_string(ctx, "print");
    }
    
    return ctx;
}

void js_eval(duk_context *ctx, const char *code) {
    if (!ctx || !code) return;
    
    duk_push_string(ctx, code);
    if (duk_peval(ctx) != 0) {
        kprintf("[JS Error] %s\n", duk_safe_to_string(ctx, -1));
    }
    duk_pop(ctx);
}

void js_destroy(duk_context *ctx) {
    if (ctx) {
        duk_destroy_heap(ctx);
    }
}
