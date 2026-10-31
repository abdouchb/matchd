#include "net/http.h"
#include "net/dns_manager.h"
#include "lwip/tcp.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"
#include "drivers/serial.h"

// mbedTLS headers
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/platform.h"

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;
static bool mbedtls_initialized = false;

// Hardware entropy stub for ZarqaaOS (insecure!)
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    (void)data;
    static uint32_t seed = 0x87654321;
    for (size_t i = 0; i < len; i++) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        output[i] = (unsigned char)seed;
    }
    *olen = len;
    return 0;
}

typedef struct {
    http_callback_t callback;
    void *callback_arg;
    struct tcp_pcb *pcb;
    char *response_buffer;
    int response_size;
    int response_capacity;
    char host[128];
    char path[256];
    uint16_t port;
    int redirects;
    
    // HTTPS fields
    bool is_https;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    char *rx_buffer;
    int rx_len;
    int rx_cap;
    bool closing;
    int refcount;
} http_state_t;

static void http_state_ref(http_state_t *s) { if (s) s->refcount++; }
static void http_state_unref(http_state_t *s) {
    if (!s) return;
    s->refcount--;
    if (s->refcount <= 0) {
        if (s->response_buffer) kfree(s->response_buffer);
        if (s->is_https) {
            mbedtls_ssl_free(&s->ssl);
            mbedtls_ssl_config_free(&s->conf);
            if (s->rx_buffer) kfree(s->rx_buffer);
        }
        kfree(s);
    }
}

static char *http_strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (*haystack != *needle) continue;
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

static void http_get_internal(const parsed_url_t *url, http_callback_t callback, void *arg, int redirects);

static void http_client_close(http_state_t *state, int status_code) {
    if (!state || state->closing) return;
    state->closing = true;

    if (state->is_https) {
        mbedtls_ssl_free(&state->ssl);
        mbedtls_ssl_config_free(&state->conf);
        if (state->rx_buffer) kfree(state->rx_buffer);
        state->rx_buffer = NULL;
    }
    
    if (state->pcb) {
        tcp_arg(state->pcb, NULL);
        tcp_sent(state->pcb, NULL);
        tcp_recv(state->pcb, NULL);
        tcp_err(state->pcb, NULL);
        tcp_poll(state->pcb, NULL, 0);
        err_t e = tcp_close(state->pcb);
        kprintf("[HTTP] tcp_close returned %d (pcb=%p)\n", e, state->pcb);
        state->pcb = NULL;
        
        // The PCB no longer holds a reference
        http_state_unref(state);
    }
    
    // Parse HTTP status code from response if we got any data
    int parsed_status = 0;
    if (state->response_buffer && state->response_size > 0) {
        char *status_line = http_strstr(state->response_buffer, "HTTP/");
        if (status_line) {
            char *space = strchr(status_line, ' ');
            if (space) {
                space++;
                while (*space >= '0' && *space <= '9') {
                    parsed_status = parsed_status * 10 + (*space - '0');
                    space++;
                }
            }
        }
    }
    
    if (parsed_status > 0) {
        status_code = parsed_status;
    }

    char tel[256];
    snprintf(tel, sizeof(tel), "{\"type\":\"net\",\"action\":\"close\",\"host\":\"%s\",\"status\":%d}", state->host, status_code);
    telemetry_emit(tel);

    if (state->response_buffer && (status_code == 301 || status_code == 302 || status_code == 307 || status_code == 308)) {
        char *loc = http_strstr(state->response_buffer, "Location: ");
        if (!loc) loc = http_strstr(state->response_buffer, "location: ");
        if (loc) {
            loc += 10;
            char *end_r = strchr(loc, '\r');
            char *end_n = strchr(loc, '\n');
            char *end = (end_r && (!end_n || end_r < end_n)) ? end_r : end_n;
            if (end) {
                char raw_location[256];
                int len = end - loc;
                if (len >= (int)sizeof(raw_location)) len = (int)sizeof(raw_location) - 1;
                strncpy(raw_location, loc, len);
                raw_location[len] = '\0';
                
                // Build the final URL to redirect to
                char new_url_str[512];
                
                if (raw_location[0] == '/') {
                    // Relative redirect: reconstruct full URL from current host
                    // e.g. Location: /search → http(s)://google.com/search
                    snprintf(new_url_str, sizeof(new_url_str), "%s://%s%s",
                             state->is_https ? "https" : "http",
                             state->host,
                             raw_location);
                    kprintf("[HTTP] Resolved relative redirect to: %s\n", new_url_str);
                } else {
                    // Absolute redirect: use as-is
                    strncpy(new_url_str, raw_location, sizeof(new_url_str) - 1);
                    new_url_str[sizeof(new_url_str) - 1] = '\0';
                }
                
                int next_redirects = state->redirects + 1;
                if (next_redirects <= 5) {
                    kprintf("[HTTP] Redirecting to: %s (count: %d)\n", new_url_str, next_redirects);
                    parsed_url_t next_url;
                    if (url_parse(new_url_str, &next_url)) {
                        http_callback_t cb = state->callback;
                        void *cb_arg = state->callback_arg;
                        
                        http_state_unref(state);
                        
                        http_get_internal(&next_url, cb, cb_arg, next_redirects);
                        return;
                    }
                } else {
                    kprintf("[HTTP] Too many redirects, aborting redirect to: %s\n", new_url_str);
                }
            }
        }
    }

    if (state->callback) {
        if (state->response_buffer) {
            char *body = http_strstr(state->response_buffer, "\r\n\r\n");
            int offset = 4;
            if (!body) {
                body = http_strstr(state->response_buffer, "\n\n");
                offset = 2;
            }
            
            if (body) {
                body += offset;
                int body_len = state->response_size - (body - state->response_buffer);
                
                // Very basic Chunked Decoder if header contains Transfer-Encoding: chunked
                if (http_strstr(state->response_buffer, "Transfer-Encoding: chunked") || http_strstr(state->response_buffer, "transfer-encoding: chunked")) {
                    int dst_len = 0;
                    char *src = body;
                    char *dst = body;
                    
                    while (src < body + body_len) {
                        // Read chunk size (hex)
                        int chunk_size = 0;
                        while (src < body + body_len && *src != '\r') {
                            char c = *src++;
                            if (c >= '0' && c <= '9') chunk_size = chunk_size * 16 + (c - '0');
                            else if (c >= 'a' && c <= 'f') chunk_size = chunk_size * 16 + (c - 'a' + 10);
                            else if (c >= 'A' && c <= 'F') chunk_size = chunk_size * 16 + (c - 'A' + 10);
                        }
                        
                        // Skip \r\n
                        if (src + 1 < body + body_len && src[0] == '\r' && src[1] == '\n') src += 2;
                        
                        if (chunk_size == 0) break; // EOF Chunk
                        
                        // Copy chunk data
                        if (src + chunk_size <= body + body_len) {
                            memmove(dst, src, chunk_size);
                            dst += chunk_size;
                            dst_len += chunk_size;
                            src += chunk_size;
                        } else {
                            int rem = (body + body_len) - src;
                            memmove(dst, src, rem);
                            dst += rem;
                            dst_len += rem;
                            src += rem;
                        }
                        
                        // Skip chunk trailing \r\n
                        if (src + 1 < body + body_len && src[0] == '\r' && src[1] == '\n') src += 2;
                    }
                    body_len = dst_len;
                    body[body_len] = '\0';
                }
                
                state->callback(body, body_len, status_code, state->callback_arg);
            } else {
                state->callback(state->response_buffer, state->response_size, status_code, state->callback_arg);
            }
        } else {
            state->callback(NULL, 0, status_code ? status_code : -1, state->callback_arg);
        }
    }

    http_state_unref(state);
}

// --- mbedTLS BIO Callbacks ---

static int my_send(void *ctx, const unsigned char *buf, size_t len) {
    struct tcp_pcb *pcb = (struct tcp_pcb *)ctx;
    if (!pcb) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    
    uint16_t send_len = len;
    if (send_len > tcp_sndbuf(pcb)) {
        send_len = tcp_sndbuf(pcb);
    }
    
    if (send_len == 0) return MBEDTLS_ERR_SSL_WANT_WRITE;
    
    err_t err = tcp_write(pcb, buf, send_len, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) {
        if (tcp_output(pcb) != ERR_OK) return MBEDTLS_ERR_SSL_WANT_WRITE;
        return send_len;
    } else if (err == ERR_MEM || err == ERR_BUF) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    } else {
        kprintf("[TLS BIO] tcp_write failed: %d\n", err);
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
}

static int my_recv(void *ctx, unsigned char *buf, size_t len) {
    http_state_t *state = (http_state_t *)ctx;
    if (state->rx_len == 0) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    
    int copy_len = len;
    if (copy_len > state->rx_len) copy_len = state->rx_len;
    
    memcpy(buf, state->rx_buffer, copy_len);
    
    state->rx_len -= copy_len;
    if (state->rx_len > 0) {
        memmove(state->rx_buffer, state->rx_buffer + copy_len, state->rx_len);
    }
    return copy_len;
}

// -----------------------------

static err_t http_recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    http_state_t *state = (http_state_t *)arg;
    
    if (p == NULL) {
        // connection closed by remote
        http_client_close(state, 0);
        return ERR_OK;
    }
    
    if (state->is_https) {
        // Buffer raw TLS packets
        if (state->rx_len + p->tot_len > state->rx_cap) {
            state->rx_cap = state->rx_len + p->tot_len + 4096;
            char *new_buf = kmalloc(state->rx_cap);
            if (state->rx_buffer) {
                memcpy(new_buf, state->rx_buffer, state->rx_len);
                kfree(state->rx_buffer);
            }
            state->rx_buffer = new_buf;
        }
        
        struct pbuf *q;
        for (q = p; q != NULL; q = q->next) {
            memcpy(state->rx_buffer + state->rx_len, q->payload, q->len);
            state->rx_len += q->len;
        }
        tcp_recved(pcb, p->tot_len);
        pbuf_free(p);
        
        // Pump TLS Handshake
        if (!mbedtls_ssl_is_handshake_over(&state->ssl)) {
            int ret = mbedtls_ssl_handshake(&state->ssl);
            if (ret == 0) {
                kprintf("[HTTP] TLS Handshake complete!\n");
                char req[512];
                strcpy(req, "GET ");
                strcat(req, state->path);
                strcat(req, " HTTP/1.1\r\nHost: ");
                strcat(req, state->host);
                strcat(req, "\r\nUser-Agent: ZarqaaBrowser/1.0\r\nAccept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
                mbedtls_ssl_write(&state->ssl, (const unsigned char *)req, strlen(req));
            } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                kprintf("[HTTP] TLS Handshake failed: -0x%04x\n", -ret);
                http_client_close(state, -4);
            }
        }
        
        // Read decrypted payload if handshake is over
        if (mbedtls_ssl_is_handshake_over(&state->ssl)) {
            unsigned char dec_buf[1024];
            int ret;
            while ((ret = mbedtls_ssl_read(&state->ssl, dec_buf, sizeof(dec_buf))) > 0) {
                if (state->response_size + ret + 1 > state->response_capacity) {
                    state->response_capacity = state->response_size + ret + 4096;
                    char *new_buf = kmalloc(state->response_capacity);
                    if (state->response_buffer) {
                        memcpy(new_buf, state->response_buffer, state->response_size);
                        kfree(state->response_buffer);
                    }
                    state->response_buffer = new_buf;
                }
                memcpy(state->response_buffer + state->response_size, dec_buf, ret);
                state->response_size += ret;
                state->response_buffer[state->response_size] = '\0';
            }
            
            // Check if we have a full HTTP response (Content-Length or Chunked terminating)
            if (state->response_buffer) {
                char *headers_end = http_strstr(state->response_buffer, "\r\n\r\n");
                if (headers_end) {
                    int headers_len = (headers_end + 4) - state->response_buffer;
                    char *cl = http_strstr(state->response_buffer, "Content-Length:");
                    if (cl && cl < headers_end) {
                        cl += 15;
                        while (*cl == ' ') cl++;
                        int content_len = 0;
                        while (*cl >= '0' && *cl <= '9') {
                            content_len = content_len * 10 + (*cl - '0');
                            cl++;
                        }
                        if (state->response_size >= headers_len + content_len) {
                            kprintf("[HTTP] Full response received (Content-Length: %d)\n", content_len);
                            http_client_close(state, 0);
                            return ERR_OK;
                        }
                    } else if (http_strstr(state->response_buffer, "\r\n0\r\n\r\n")) {
                        kprintf("[HTTP] Full response received (Chunked EOF)\n");
                        http_client_close(state, 0);
                        return ERR_OK;
                    }
                }
            }
            if (ret < 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (ret != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                    kprintf("[HTTP] TLS Read error: -0x%04x\n", -ret);
                }
                http_client_close(state, 0);
            }
        }
        return ERR_OK;
    }
    
    if (err != ERR_OK) {
        if (p) pbuf_free(p);
        return err;
    }
    
    if (state->response_size + p->tot_len + 1 >= state->response_capacity) {
        state->response_capacity = (state->response_capacity + p->tot_len + 1) * 2;
        char *new_buf = kmalloc(state->response_capacity);
        if (state->response_buffer) {
            memcpy(new_buf, state->response_buffer, state->response_size);
            kfree(state->response_buffer);
        }
        state->response_buffer = new_buf;
    }
    
    kprintf("[HTTP] HTTP response received (%d bytes)!\n", p->tot_len);
    
    struct pbuf *q;
    for (q = p; q != NULL; q = q->next) {
        memcpy(state->response_buffer + state->response_size, q->payload, q->len);
        state->response_size += q->len;
    }
    
    state->response_buffer[state->response_size] = '\0';
    
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    
    // Check if the response is fully received so we don't hang if the server keeps the connection alive
    if (state->response_buffer) {
        char *headers_end = http_strstr(state->response_buffer, "\r\n\r\n");
        if (headers_end) {
            int headers_len = (headers_end + 4) - state->response_buffer;
            char *cl = http_strstr(state->response_buffer, "Content-Length:");
            if (cl && cl < headers_end) {
                cl += 15;
                while (*cl == ' ') cl++;
                int content_len = 0;
                while (*cl >= '0' && *cl <= '9') {
                    content_len = content_len * 10 + (*cl - '0');
                    cl++;
                }
                if (state->response_size >= headers_len + content_len) {
                    kprintf("[HTTP] Full response received (Content-Length: %d)\n", content_len);
                    http_client_close(state, 0);
                    return ERR_OK;
                }
            } else if (http_strstr(state->response_buffer, "\r\n0\r\n\r\n")) {
                kprintf("[HTTP] Full response received (Chunked EOF)\n");
                http_client_close(state, 0);
                return ERR_OK;
            }
        }
    }
    
    return ERR_OK;
}

static void http_err_callback(void *arg, err_t err) {
    http_state_t *state = (http_state_t *)arg;
    kprintf("[HTTP] TCP Error callback: err=%d state=%p pcb=%p\n", err, state, state ? state->pcb : NULL);
    
    char tel[256];
    snprintf(tel, sizeof(tel), "{\"type\":\"net\",\"action\":\"error\",\"host\":\"%s\",\"err\":%d}", state ? state->host : "unknown", err);
    telemetry_emit(tel);
    
    if (state) {
        state->pcb = NULL;
        http_client_close(state, err);
        
        // The PCB no longer holds a reference because lwIP freed it
        http_state_unref(state);
    }
}

static err_t http_connected_callback(void *arg, struct tcp_pcb *pcb, err_t err) {
    http_state_t *state = (http_state_t *)arg;
    kprintf("[HTTP] http_connected_callback: err=%d state=%p pcb=%p\n", err, state, pcb);
    
    char tel[256];
    snprintf(tel, sizeof(tel), "{\"type\":\"net\",\"action\":\"connect\",\"host\":\"%s\",\"err\":%d}", state->host, err);
    telemetry_emit(tel);
    
    if (err != ERR_OK) {
        kprintf("[HTTP] tcp_connect failed with %d\n", err);
        http_client_close(state, -3);
        return err;
    }
    
    kprintf("[HTTP] Connected, sending request...\n");
    
    if (state->is_https) {
        // Initialize mbedTLS state for this connection
        if (!mbedtls_initialized) {
            mbedtls_platform_set_calloc_free((void *(*)(size_t, size_t))kcalloc, kfree);
            mbedtls_entropy_init(&entropy);
            mbedtls_ctr_drbg_init(&ctr_drbg);
            mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"zarqaa", 6);
            mbedtls_initialized = true;
        }
        
        mbedtls_ssl_init(&state->ssl);
        mbedtls_ssl_config_init(&state->conf);
        
        mbedtls_ssl_config_defaults(&state->conf,
                                    MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
                                    
        mbedtls_ssl_conf_authmode(&state->conf, MBEDTLS_SSL_VERIFY_NONE); // Insecure, skips cert verification
        mbedtls_ssl_conf_rng(&state->conf, mbedtls_ctr_drbg_random, &ctr_drbg);
        
        mbedtls_ssl_setup(&state->ssl, &state->conf);
        mbedtls_ssl_set_hostname(&state->ssl, state->host);
        mbedtls_ssl_set_bio(&state->ssl, state, my_send, my_recv, NULL);
        
        // Start handshake
        int ret = mbedtls_ssl_handshake(&state->ssl);
        if (ret != 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            kprintf("[HTTP] TLS Initial Handshake error: -0x%04x\n", -ret);
            http_client_close(state, -4);
        }
    } else {
        char req[512];
        strcpy(req, "GET ");
        strcat(req, state->path);
        strcat(req, " HTTP/1.1\r\nHost: ");
        strcat(req, state->host);
        strcat(req, "\r\nUser-Agent: ZarqaaBrowser/1.0\r\nAccept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
        
        err_t we = tcp_write(pcb, req, strlen(req), TCP_WRITE_FLAG_COPY);
        if (we != ERR_OK) {
            kprintf("[HTTP] tcp_write failed: %d\n", we);
        } else {
            tcp_output(pcb);
        }
    }
    
    return ERR_OK;
}

static void http_dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    http_state_t *state = (http_state_t *)callback_arg;
    
    if (ipaddr == NULL) {
        kprintf("[HTTP] DNS resolution failed for %s\n", name);
        http_client_close(state, -2); // -2 is DNS error
        return;
    }
    
    kprintf("[HTTP] DNS callback name='%s' ip=%d.%d.%d.%d state->host='%s'\n", name, 
            ip4_addr1(ipaddr), ip4_addr2(ipaddr), ip4_addr3(ipaddr), ip4_addr4(ipaddr), state->host);
    
    state->pcb = tcp_new();
    if (!state->pcb) {
        http_client_close(state, -3); // -3 is out of pcbs
        return;
    }
    
    // The PCB will hold a reference to state
    http_state_ref(state);
    tcp_arg(state->pcb, state);
    tcp_err(state->pcb, http_err_callback);
    tcp_recv(state->pcb, http_recv_callback);
    
    char tel[256];
    snprintf(tel, sizeof(tel), "{\"type\":\"net\",\"action\":\"start\",\"host\":\"%s\"}", state->host);
    telemetry_emit(tel);
    
    err_t err = tcp_connect(state->pcb, ipaddr, state->port, http_connected_callback);
    kprintf("[HTTP] tcp_connect returned %d for state=%p pcb=%p\n", err, state, state->pcb);
    if (err != ERR_OK) {
        kprintf("[HTTP] tcp_connect failed synchronously %d\n", err);
        http_client_close(state, -3);
    }
}

static void http_get_internal(const parsed_url_t *url, http_callback_t callback, void *arg, int redirects) {
    http_state_t *state = kmalloc(sizeof(http_state_t));
    memset(state, 0, sizeof(http_state_t));
    
    strncpy(state->host, url->host, sizeof(state->host) - 1);
    state->host[sizeof(state->host) - 1] = '\0';
    strncpy(state->path, url->path, sizeof(state->path) - 1);
    state->path[sizeof(state->path) - 1] = '\0';
    state->port = url->port ? url->port : (url->type == URL_TYPE_HTTPS ? 443 : 80);
    state->redirects = redirects;
    state->callback = callback;
    state->callback_arg = arg;
    state->is_https = (url->type == URL_TYPE_HTTPS);
    state->refcount = 1;
    
    state->response_capacity = 8192;
    state->response_buffer = kmalloc(state->response_capacity);

    dns_manager_resolve(state->host, http_dns_found, state);
}

void http_get(const parsed_url_t *url, http_callback_t callback, void *arg) {
    http_get_internal(url, callback, arg, 0);
}
