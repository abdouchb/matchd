#include "stdio.h"
#include "zarqaa.h"
#include <stdarg.h>
#include <stdbool.h>

static void print_char(char** str, char c, size_t* count) {
    if (str && *str) {
        **str = c;
        (*str)++;
    } else {
        write(1, &c, 1);
    }
    (*count)++;
}

static void print_string(char** str, const char* s, size_t* count) {
    if (!s) s = "(null)";
    while (*s) {
        print_char(str, *s, count);
        s++;
    }
}

static void print_number(char** str, unsigned long long num, int base, bool sign, size_t* count) {
    char buf[64];
    int i = 0;
    
    if (sign && (long long)num < 0) {
        print_char(str, '-', count);
        num = -((long long)num);
    }
    
    if (num == 0) {
        buf[i++] = '0';
    } else {
        while (num > 0) {
            int rem = num % base;
            if (rem < 10) buf[i++] = '0' + rem;
            else buf[i++] = 'a' + (rem - 10);
            num /= base;
        }
    }
    
    while (i > 0) {
        print_char(str, buf[--i], count);
    }
}

static int vprintf_internal(char** str, const char* format, va_list args) {
    size_t count = 0;
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'c':
                    print_char(str, (char)va_arg(args, int), &count);
                    break;
                case 's':
                    print_string(str, va_arg(args, const char*), &count);
                    break;
                case 'd':
                case 'i':
                    print_number(str, va_arg(args, int), 10, true, &count);
                    break;
                case 'u':
                    print_number(str, va_arg(args, unsigned int), 10, false, &count);
                    break;
                case 'x':
                case 'X':
                    print_number(str, va_arg(args, unsigned int), 16, false, &count);
                    break;
                case 'p':
                    print_string(str, "0x", &count);
                    print_number(str, (unsigned long long)va_arg(args, void*), 16, false, &count);
                    break;
                case '%':
                    print_char(str, '%', &count);
                    break;
                default:
                    print_char(str, '%', &count);
                    print_char(str, *format, &count);
                    break;
            }
        } else {
            print_char(str, *format, &count);
        }
        format++;
    }
    
    if (str && *str) {
        **str = '\0';
    }
    
    return count;
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vprintf_internal(NULL, format, args);
    va_end(args);
    return ret;
}

int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vprintf_internal(&str, format, args);
    *str = '\0';
    va_end(args);
    return ret;
}

int getchar(void) {
    char c = 0;
    if (read(0, &c, 1) > 0) {
        return (unsigned char)c;
    }
    return -1;
}

char* gets(char* str, int max_len) {
    if (!str || max_len <= 0) return NULL;
    int idx = 0;
    while (idx < max_len - 1) {
        int c = getchar();
        if (c == -1) continue;
        
        if (c == '\n' || c == '\r') {
            printf("\n");
            break;
        } else if (c == '\b') {
            if (idx > 0) {
                idx--;
                printf("\b \b");
            }
        } else {
            str[idx++] = (char)c;
            char echo[2] = {(char)c, 0};
            printf("%s", echo);
        }
    }
    str[idx] = '\0';
    return str;
}
