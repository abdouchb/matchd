/*
 * Zarqaa OS 0.1 - POSIX time.h Compat stub
 * Creator: Abdelhak Chabbi
 */

#ifndef COMPAT_TIME_H
#define COMPAT_TIME_H

#include "zarqaa/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef long time_t;
typedef long clock_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

time_t time(time_t *tloc);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm *timeptr);
struct tm *localtime(const time_t *timer);
struct tm *gmtime(const time_t *timer);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);

#ifdef __cplusplus
}
#endif

#endif /* COMPAT_TIME_H */
