#include "resource/resource_manager.h"
#include "net/http.h"
#include "filesystem/vfs.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

// State for HTTP→HTTPS auto-upgrade
typedef struct {
    resource_callback_t user_cb;
    void *user_arg;
    char host[256];
    char path[256];
    bool tried_https_upgrade;
} resource_http_state_t;

static void http_resource_cb(const char *body, int len, int status_code, void *arg) {
    resource_http_state_t *rs = (resource_http_state_t *)arg;
    
    // If HTTP failed (empty response, connection abort, or RST) and we haven't
    // tried HTTPS yet, automatically retry with HTTPS
    if (!rs->tried_https_upgrade && (body == NULL || len <= 0) && 
        (status_code <= 0 || status_code == -13 || status_code == -1)) {
        
        kprintf("[Resource] HTTP failed (status=%d), upgrading to HTTPS for %s\n", status_code, rs->host);
        
        rs->tried_https_upgrade = true;
        
        parsed_url_t https_url;
        memset(&https_url, 0, sizeof(https_url));
        https_url.type = URL_TYPE_HTTPS;
        https_url.secure = true;
        https_url.port = 443;
        strcpy(https_url.scheme, "https");
        strncpy(https_url.host, rs->host, sizeof(https_url.host) - 1);
        strncpy(https_url.path, rs->path, sizeof(https_url.path) - 1);
        snprintf(https_url.full_url, sizeof(https_url.full_url), "https://%s%s", rs->host, rs->path);
        
        http_get(&https_url, http_resource_cb, rs);
        return; // Don't free rs yet, it will be freed in the HTTPS callback
    }
    
    // Deliver result to the browser
    rs->user_cb(body, len, rs->user_arg);
    kfree(rs);
}

void resource_load(const parsed_url_t *url, resource_callback_t callback, void *arg) {
    if (!url || !callback) return;
    
    if (url->type == URL_TYPE_LOCAL) {
        struct vfs_node *node = vfs_open(url->path);
        if (node && node->size > 0) {
            char *buf = kmalloc(node->size + 1);
            if (buf) {
                int read = vfs_read(node, buf, node->size, 0);
                buf[read] = '\0';
                callback(buf, read, arg);
                kfree(buf);
            } else {
                callback(NULL, 0, arg);
            }
        } else {
            callback(NULL, -1, arg);
        }
    } else if (url->type == URL_TYPE_HTTP || url->type == URL_TYPE_HTTPS) {
        resource_http_state_t *rs = kmalloc(sizeof(resource_http_state_t));
        memset(rs, 0, sizeof(resource_http_state_t));
        rs->user_cb = callback;
        rs->user_arg = arg;
        strncpy(rs->host, url->host, sizeof(rs->host) - 1);
        strncpy(rs->path, url->path, sizeof(rs->path) - 1);
        // If already HTTPS, don't try upgrade again
        rs->tried_https_upgrade = (url->type == URL_TYPE_HTTPS);
        
        http_get(url, http_resource_cb, rs);
    } else {
        callback(NULL, -2, arg);
    }
}
