/*
 * Zarqaa OS 0.1 - POSIX unistd.h Compat
 * Creator: Abdelhak Chabbi
 */

#ifndef COMPAT_UNISTD_H
#define COMPAT_UNISTD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long ssize_t;

int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* COMPAT_UNISTD_H */
