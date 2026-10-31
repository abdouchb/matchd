/*
 * Zarqaa OS 0.1 - String Library
 * Creator: Abdelhak Chabbi
 */

#include "zarqaa/string.h"

void *memset(void *s, int c, size_t n) {
    uint8_t *p = s;
    uint8_t uc = (uint8_t)c;
    uint64_t c64 = uc | ((uint64_t)uc << 8) | ((uint64_t)uc << 16) | ((uint64_t)uc << 24) |
                   ((uint64_t)uc << 32) | ((uint64_t)uc << 40) | ((uint64_t)uc << 48) | ((uint64_t)uc << 56);

    uint64_t *p64 = (uint64_t *)p;
    size_t n64 = n / 8;
    while (n64--) {
        *p64++ = c64;
    }
    
    p = (uint8_t *)p64;
    size_t n8 = n % 8;
    while (n8--) {
        *p++ = uc;
    }
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    
    uint64_t *d64 = (uint64_t *)d;
    const uint64_t *s64 = (const uint64_t *)s;
    
    size_t n64 = n / 8;
    while (n64--) {
        *d64++ = *s64++;
    }
    
    d = (uint8_t *)d64;
    s = (const uint8_t *)s64;
    size_t n8 = n % 8;
    while (n8--) {
        *d++ = *s++;
    }
    return dest;
}

void *memset32(void *s, uint32_t c, size_t n_words) {
    uint32_t *p = (uint32_t *)s;
    while (n_words--) {
        *p++ = c;
    }
    return s;
}

void *memcpy64(void *dest, const void *src, size_t n_qwords) {
    uint64_t *d = (uint64_t *)dest;
    const uint64_t *s = (const uint64_t *)src;
    while (n_qwords--) {
        *d++ = *s++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        char *lasts = (char *)s + (n - 1);
        char *lastd = d + (n - 1);
        while (n--) {
            *lastd-- = *lasts--;
        }
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    unsigned char uc = (unsigned char)c;
    while (n--) {
        if (*p == uc) {
            return (void *)p;
        }
        p++;
    }
    return NULL;
}

static unsigned long next_rand = 1;

int rand(void) {
    next_rand = next_rand * 1103515245 + 12345;
    return (unsigned int)(next_rand / 65536) % 32768;
}

void srand(unsigned int seed) {
    next_rand = seed;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;

    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;

        while (*h && *n && *h == *n) {
            h++;
            n++;
        }

        if (!*n) return (char *)haystack;
    }

    return NULL;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (*d) d++;
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    *d = '\0';
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s != (char)c) {
        if (!*s++) return NULL;
    }
    return (char *)s;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    do {
        if (*s == (char)c) {
            last = s;
        }
    } while (*s++);
    return (char *)last;
}

static void reverse(char *str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void itoa(int value, char *str, int base) {
    int i = 0;
    bool isNegative = false;

    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    if (value < 0 && base == 10) {
        isNegative = true;
        value = -value;
    }

    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }

    if (isNegative) {
        str[i++] = '-';
    }

    str[i] = '\0';
    reverse(str, i);
}

void utoa(unsigned int value, char *str, int base) {
    int i = 0;

    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }

    str[i] = '\0';
    reverse(str, i);
}

int atoi(const char *str) {
    int res = 0;
    int sign = 1;
    int i = 0;

    if (str[0] == '-') {
        sign = -1;
        i++;
    }

    for (; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9') {
            res = res * 10 + str[i] - '0';
        } else {
            break;
        }
    }

    return sign * res;
}

void itoa64(long long value, char *str, int base) {
    int i = 0;
    bool isNegative = false;

    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    if (value < 0 && base == 10) {
        isNegative = true;
        value = -value;
    }

    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }

    if (isNegative) {
        str[i++] = '-';
    }

    str[i] = '\0';
    reverse(str, i);
}

void utoa64(unsigned long long value, char *str, int base) {
    int i = 0;

    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }

    str[i] = '\0';
    reverse(str, i);
}
