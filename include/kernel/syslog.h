#ifndef SYSLOG_H
#define SYSLOG_H

#include <stdint.h>
#include <stddef.h>

void syslog_init(void);
void syslog_write(const char *msg);
void syslog_flush_to_disk(void);

// For the Settings app to read
char* syslog_get_buffer(void);
size_t syslog_get_length(void);

#endif
