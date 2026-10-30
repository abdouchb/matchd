/*
 * Zarqaa OS 0.1 - JavaScript Engine Wrapper Header
 * Creator: Abdelhak Chabbi
 */

#ifndef KERNEL_JS_ENGINE_H
#define KERNEL_JS_ENGINE_H

#include "zarqaa_duk_shim.h"
#include "duktape.h"

duk_context *js_init(void);
void js_eval(duk_context *ctx, const char *code);
void js_destroy(duk_context *ctx);

#endif /* KERNEL_JS_ENGINE_H */
