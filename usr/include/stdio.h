#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>

int printf(const char* format, ...);
int sprintf(char* str, const char* format, ...);

int getchar(void);
char* gets(char* str, int max_len);

#endif
