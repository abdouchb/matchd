/*
 * Zarqaa OS 0.1 - POSIX setjmp.h Compat stub
 * Creator: Abdelhak Chabbi
 */

#ifndef COMPAT_SETJMP_H
#define COMPAT_SETJMP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Standard x86_64 jmp_buf size */
typedef unsigned long long jmp_buf[8];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#ifdef __cplusplus
}
#endif

#endif /* COMPAT_SETJMP_H */
