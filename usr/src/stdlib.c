#include "stdlib.h"

int atoi(const char* str) {
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

char* itoa(int value, char* str, int base) {
    if (base < 2 || base > 36) {
        str[0] = '\0';
        return str;
    }
    
    int i = 0;
    int isNegative = 0;
    
    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }
    
    if (value < 0 && base == 10) {
        isNegative = 1;
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
    
    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
    
    return str;
}
