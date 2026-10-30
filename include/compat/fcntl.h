/*
 * Zarqaa OS 0.1 - POSIX fcntl.h Compat
 * Creator: Abdelhak Chabbi
 */

#ifndef COMPAT_FCNTL_H
#define COMPAT_FCNTL_H

#ifdef __cplusplus
extern "C" {
#endif

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

int open(const char *pathname, int flags, ...);

#ifdef __cplusplus
}
#endif

#endif /* COMPAT_FCNTL_H */
