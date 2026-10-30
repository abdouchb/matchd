#include "resource.h"
#include "kernel/heap.h"
#include "filesystem/vfs.h"
#include "zarqaa/string.h"

void browser_resource_manager_init(void) {
    // Initialize cache structures here later
}

resource_t *browser_resource_fetch(const char *url) {
    if (!url || strlen(url) == 0) return NULL;
    
    resource_t *res = kcalloc(1, sizeof(resource_t));
    strcpy(res->url, url);
    res->type = RES_TYPE_UNKNOWN;
    
    // Naive local file loading (to be expanded with Network/Cache)
    struct vfs_node *file = vfs_open(url);
    if (file && file->type == VFS_FILE && file->size > 0) {
        res->data = kcalloc(1, file->size + 1);
        int read_bytes = vfs_read(file, res->data, file->size, 0);
        if (read_bytes > 0) {
            res->size = read_bytes;
            res->is_loaded = true;
            
            // Determine type
            size_t url_len = strlen(url);
            if (url_len > 5 && strcmp(url + url_len - 5, ".html") == 0) {
                res->type = RES_TYPE_HTML;
            } else if (url_len > 4 && strcmp(url + url_len - 4, ".htm") == 0) {
                res->type = RES_TYPE_HTML;
            } else if (url_len > 4 && strcmp(url + url_len - 4, ".css") == 0) {
                res->type = RES_TYPE_CSS;
            }
        } else {
            kfree(res->data);
            res->data = NULL;
        }
    }
    
    return res;
}

void browser_resource_free(resource_t *res) {
    if (!res) return;
    if (res->data) kfree(res->data);
    kfree(res);
}
