/*
 * Zarqaa OS 0.1 - String Library Header
 * Creator: Abdelhak Chabbi
 */

#ifndef ZARQAA_STRING_H
#define ZARQAA_STRING_H

#include "zarqaa/types.h"

/* Memory operations */
void *memset(void *s, int c, size_t n);
void *memset32(void *s, uint32_t c, size_t n_words);
void *memcpy(void *dest, const void *src, size_t n);
void *memcpy64(void *dest, const void *src, size_t n_qwords);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);

/* String operations */
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);

/* Conversion */
void itoa(int value, char *str, int base);
void utoa(unsigned int value, char *str, int base);
void itoa64(long long value, char *str, int base);
void utoa64(unsigned long long value, char *str, int base);
int atoi(const char *str);

/* Format String */
int snprintf(char *str, size_t size, const char *format, ...);
int sprintf(char *str, const char *format, ...);

#endif /* ZARQAA_STRING_H */
