#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "zarqaa/types.h"
#include "zarqaa/string.h"

#define LWIP_ERR_T int
#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"

extern void kprintf(const char *fmt, ...);
extern void panic(const char *msg);

#define LWIP_PLATFORM_DIAG(x)   do { kprintf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { panic(x); } while(0)

#define BYTE_ORDER LITTLE_ENDIAN

/* Tell lwip we have stdint/stddef/ssize_t */
#define LWIP_NO_STDINT_H 0
#define LWIP_NO_STDDEF_H 0
#define LWIP_NO_UNISTD_H 1
#define SSIZE_MAX 0x7FFFFFFFFFFFFFFF

/* rand() mapping */
extern int net_rand(int range);
#define LWIP_RAND() ((uint32_t)net_rand(0x7FFFFFFF))

#endif /* LWIP_ARCH_CC_H */
