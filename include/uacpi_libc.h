#ifndef UACPI_LIBC_H
#define UACPI_LIBC_H

#include "zarqaa/string.h"
#include "zarqaa/types.h"

#define uacpi_memcpy  memcpy
#define uacpi_memmove memmove
#define uacpi_memset  memset
#define uacpi_memcmp  memcmp
#define uacpi_strcmp  strcmp
#define uacpi_strlen  strlen
#define uacpi_strnlen strnlen
#define uacpi_snprintf snprintf
#define uacpi_vsnprintf vsnprintf

#endif
