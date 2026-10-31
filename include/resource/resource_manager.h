#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "net/url.h"

#define RESOURCE_LOAD_ERROR -1

typedef void (*resource_callback_t)(const char *data, int len, void *arg);

void resource_load(const parsed_url_t *url, resource_callback_t callback, void *arg);

#endif /* RESOURCE_MANAGER_H */
