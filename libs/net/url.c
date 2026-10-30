#include "net/url.h"
#include "zarqaa/string.h"

bool url_parse(const char *url_str, parsed_url_t *out_url) {
    if (!url_str || !out_url) return false;
    memset(out_url, 0, sizeof(parsed_url_t));

    const char *p = url_str;
    
    // Trim leading spaces
    while (*p == ' ') p++;
    if (*p == '\0') return false;

    // Default to HTTP
    out_url->type = URL_TYPE_HTTP;
    out_url->secure = false;
    out_url->port = 80;

    // Check scheme
    if (strncmp(p, "http://", 7) == 0) {
        out_url->type = URL_TYPE_HTTP;
        strcpy(out_url->scheme, "http");
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        out_url->type = URL_TYPE_HTTPS;
        strcpy(out_url->scheme, "https");
        out_url->secure = true;
        out_url->port = 443;
        p += 8;
    } else if (strncmp(p, "file://", 7) == 0) {
        out_url->type = URL_TYPE_LOCAL;
        strcpy(out_url->scheme, "file");
        p += 7;
        strcpy(out_url->host, "");
        strcpy(out_url->path, p);
        strcpy(out_url->full_url, url_str);
        return true;
    } else if (strncmp(p, "zarqaa://", 9) == 0) {
        out_url->type = URL_TYPE_INTERNAL;
        strcpy(out_url->scheme, "zarqaa");
        p += 9;
        strcpy(out_url->host, "");
        strcpy(out_url->path, p);
        strcpy(out_url->full_url, url_str);
        return true;
    } else if (*p == '/') {
        out_url->type = URL_TYPE_LOCAL;
        strcpy(out_url->scheme, "file");
        strcpy(out_url->host, "");
        strcpy(out_url->path, p);
        
        strcpy(out_url->full_url, "file://");
        strcat(out_url->full_url, p);
        return true;
    } else {
        // No scheme, assume http://
        strcpy(out_url->scheme, "http");
    }

    // Extract host and path
    const char *slash = strchr(p, '/');
    if (slash) {
        int host_len = slash - p;
        if (host_len >= sizeof(out_url->host)) host_len = sizeof(out_url->host) - 1;
        strncpy(out_url->host, p, host_len);
        out_url->host[host_len] = '\0';
        strncpy(out_url->path, slash, sizeof(out_url->path) - 1);
        out_url->path[sizeof(out_url->path) - 1] = '\0';
    } else {
        strncpy(out_url->host, p, sizeof(out_url->host) - 1);
        out_url->host[sizeof(out_url->host) - 1] = '\0';
        strcpy(out_url->path, "/");
    }

    // Check for port in host string
    char *colon = strchr(out_url->host, ':');
    if (colon) {
        *colon = '\0';
        int custom_port = 0;
        char *port_str = colon + 1;
        while (*port_str >= '0' && *port_str <= '9') {
            custom_port = custom_port * 10 + (*port_str - '0');
            port_str++;
        }
        if (custom_port > 0) {
            out_url->port = custom_port;
        }
    }

    // Special case: if domain is missing a dot and is not localhost, try duckduckgo
    if (strchr(out_url->host, '.') == NULL && strcmp(out_url->host, "localhost") != 0) {
        out_url->type = URL_TYPE_HTTPS;
        out_url->secure = true;
        strcpy(out_url->scheme, "https");
        strcpy(out_url->host, "duckduckgo.com");
        strcpy(out_url->path, "/?q=");
        strncat(out_url->path, p, sizeof(out_url->path) - 5);
        out_url->path[sizeof(out_url->path) - 1] = '\0';
    }

    // Reconstruct full normalized URL
    strcpy(out_url->full_url, out_url->scheme);
    strcat(out_url->full_url, "://");
    strcat(out_url->full_url, out_url->host);
    
    if ((out_url->type == URL_TYPE_HTTP && out_url->port != 80) || 
        (out_url->type == URL_TYPE_HTTPS && out_url->port != 443)) {
        char port_buf[16];
        snprintf(port_buf, sizeof(port_buf), ":%d", out_url->port);
        strcat(out_url->full_url, port_buf);
    }
    
    if (strcmp(out_url->path, "/") != 0 || slash != NULL) {
        strncat(out_url->full_url, out_url->path, sizeof(out_url->full_url) - strlen(out_url->full_url) - 1);
    } else {
        strcat(out_url->full_url, "/");
    }
    out_url->full_url[sizeof(out_url->full_url) - 1] = '\0';
    
    return true;
}
