/*
 * Zarqaa OS - Duktape time.h stub
 *
 * This file exists solely to satisfy #include <time.h> inside duk_config.h.
 * All actual time definitions (time_t, struct tm, etc.) are already provided
 * by zarqaa_duk_shim.h which is force-included BEFORE this via -include flag.
 *
 * Guard matches zarqaa_duk_shim.h's COMPAT_TIME_H to prevent any double-definition.
 */

#ifndef COMPAT_TIME_H
/* Should never reach here if shim was included first, but define guard anyway */
#define COMPAT_TIME_H

typedef long long time_t;
typedef long long clock_t;

struct tm {
    int tm_sec, tm_min, tm_hour;
    int tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
};

static inline time_t time(time_t *t)                    { if (t) *t = 0; return 0; }
static inline double difftime(time_t a, time_t b)       { return (double)(a - b); }
static inline time_t mktime(struct tm *t)               { (void)t; return 0; }
static inline struct tm *gmtime(const time_t *t)        { (void)t; return (void*)0; }
static inline struct tm *localtime(const time_t *t)     { (void)t; return (void*)0; }

#endif /* COMPAT_TIME_H */
