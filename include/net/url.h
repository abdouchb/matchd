#ifndef NET_URL_H
#define NET_URL_H

#include "zarqaa/types.h"
#include <stdbool.h>

typedef enum {
    URL_TYPE_LOCAL,
    URL_TYPE_HTTP,
    URL_TYPE_HTTPS,
    URL_TYPE_INTERNAL,
    URL_TYPE_INVALID
} url_type_t;

typedef struct {
    url_type_t type;
    char scheme[16];
    char host[256];
    char path[1024];
    char full_url[1280]; // Normalized full URL
    uint16_t port;
    bool secure;
} parsed_url_t;

/* Parses and normalizes a given URL string */
bool url_parse(const char *url_str, parsed_url_t *out_url);

#endif /* NET_URL_H */
