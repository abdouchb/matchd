#ifndef NET_HTTP_H
#define NET_HTTP_H

#include "net/url.h"

typedef void (*http_callback_t)(const char *response_body, int body_len, int status_code, void *arg);

void http_get(const parsed_url_t *url, http_callback_t callback, void *arg);

#endif /* NET_HTTP_H */
