#ifndef BROWSER_RESOURCE_H
#define BROWSER_RESOURCE_H

#include "zarqaa/types.h"

typedef enum {
    RES_TYPE_HTML,
    RES_TYPE_CSS,
    RES_TYPE_IMAGE,
    RES_TYPE_FONT,
    RES_TYPE_UNKNOWN
} resource_type_t;

typedef struct {
    char url[256];
    resource_type_t type;
    void *data;
    size_t size;
    bool is_loaded;
} resource_t;

// Initialize the resource manager (and its cache)
void browser_resource_manager_init(void);

// Fetch a resource (from memory cache, network, or filesystem)
resource_t *browser_resource_fetch(const char *url);

// Free a resource if not cached
void browser_resource_free(resource_t *res);

#endif
