#include "browser.h"
#include "kernel/heap.h"
#include "filesystem/vfs.h"
#include "zarqaa/string.h"
#include "../platform/canvas.h"
#include "../platform/resource.h"
#include "../platform/scheduler.h"
#include "../platform/theme.h"
#include "renderer.h"
#include "drivers/network.h"

static bool browser_network_ready = false;
static char browser_pending_url[MAX_URL_LEN] = {0};
static browser_context_t *global_ctx = NULL;

static void browser_network_listener(network_event_t event) {
    if (event == NETWORK_EVENT_READY) {
        browser_network_ready = true;
        if (browser_pending_url[0] != '\0' && global_ctx) {
            kprintf("[Browser] Network ready, retrying pending URL: %s\n", browser_pending_url);
            browser_load_url(global_ctx, browser_pending_url);
            browser_pending_url[0] = '\0';
        }
    } else if (event == NETWORK_EVENT_DISCONNECTED) {
        browser_network_ready = false;
    }
}

void browser_trigger_redraw(void) {
    if (global_ctx && global_ctx->win) {
        global_ctx->win->needs_redraw = true;
    }
}

const char *browser_get_current_url(void) {
    if (!global_ctx || global_ctx->tab_count == 0) return "";
    browser_tab_t *tab = &global_ctx->tabs[global_ctx->active_tab_index];
    if (tab->history_count > 0) {
        return tab->history[tab->history_index].url;
    }
    return "";
}

void browser_init(struct window *win) {
    browser_context_t *ctx = kcalloc(1, sizeof(browser_context_t));
    global_ctx = ctx;
    ctx->win = win;
    
    ctx->sidebar_width = 0;
    ctx->omnibox_width = 600;
    ctx->omnibox_height = 40;
    ctx->statusbar_height = 24;
    ctx->is_omnibox_focused = false;
    
    // Initialize Platform
    browser_theme_init();
    browser_scheduler_init();
    browser_resource_manager_init();
    
    network_register_listener(browser_network_listener);
    
    // Setup Canvas
    surface_t *win_surf = kcalloc(1, sizeof(surface_t));
    win_surf->width = win->width;
    win_surf->height = win->height;
    win_surf->pitch = win->width * 4;
    win_surf->format = PIXEL_FORMAT_ARGB32;
    win_surf->cpu_pointer = win->content_buffer;
    win_surf->pixels = win->content_buffer;
    ctx->canvas = canvas_create(win_surf);
    
    browser_renderer_init();
    
    // Initialize Tabs array
    ctx->next_tab_id = 1;
    ctx->tabs[0].id = ctx->next_tab_id++;
    ctx->tabs[0].history_count = 0;
    ctx->tabs[0].history_index = -1;
    ctx->tabs[0].scroll_y = 0;
    ctx->tab_count = 1;
    ctx->active_tab_index = 0;
    
    // Initial Bookmarks setup
    strncpy(ctx->bookmarks[0].url, "google.com", MAX_URL_LEN);
    strncpy(ctx->bookmarks[0].title, "Google", 100);
    strncpy(ctx->bookmarks[1].url, "example.com", MAX_URL_LEN);
    strncpy(ctx->bookmarks[1].title, "Example", 100);
    ctx->bookmark_count = 2;
    
    // Initialize JS Engine!
    ctx->js_ctx = js_init();
    
    win->user_data = ctx;
}

#include "resource/resource_manager.h"
#include "net/url.h"

typedef struct {
    browser_context_t *ctx;
    browser_tab_t *tab;
    char url[MAX_URL_LEN];
} browser_load_arg_t;

static void browser_resource_loaded_cb(const char *data, int len, void *arg) {
    browser_load_arg_t *load_arg = (browser_load_arg_t *)arg;
    browser_context_t *ctx = load_arg->ctx;
    browser_tab_t *tab = load_arg->tab;

    if (data && len > 0) {
        kprintf("[Browser] resource loaded, len=%d. First 50 bytes:\n", len);
        for(int i=0; i<50 && i<len; i++) {
            kprintf("%c", data[i] < 32 && data[i] != '\n' && data[i] != '\r' ? '.' : data[i]);
        }
        kprintf("\n");
        
        tab->document = browser_renderer_parse_html(data, len);
        if (tab->document) {
            tab->layout_root = browser_renderer_build_layout(tab->document);
            if (tab->layout_root) {
                browser_renderer_compute_layout(tab->layout_root, ctx->sidebar_width + 10, 80, ctx->win->width - ctx->sidebar_width - 30);
            } else {
                kprintf("[Browser] build_layout returned NULL!\n");
            }
            
            // Run JavaScript: handle both inline <script> and <script src="...">
            if (ctx->js_ctx) {
                /* Walk raw HTML to find all script tags */
                const char *p = data;
                while ((p = strstr(p, "<script")) != NULL) {
                    p += 7; /* skip "<script" */
                    /* Find end of opening tag */
                    const char *tag_end = strchr(p, '>');
                    if (!tag_end) break;
                    
                    /* Check for src="..." attribute in the opening tag */
                    bool has_src = false;
                    const char *src_start = NULL;
                    size_t src_len = 0;
                    
                    char attr_buf[256] = {0};
                    int attr_buf_len = (int)(tag_end - p);
                    if (attr_buf_len > 255) attr_buf_len = 255;
                    strncpy(attr_buf, p, attr_buf_len);
                    
                    const char *src_pos = strstr(attr_buf, "src=\"");
                    if (!src_pos) src_pos = strstr(attr_buf, "src='");
                    
                    if (src_pos) {
                        has_src = true;
                        char quote = src_pos[4];
                        src_start = src_pos + 5;
                        const char *src_end = strchr(src_start, quote);
                        if (src_end) {
                            src_len = src_end - src_start;
                            if (src_len > 0 && src_len < 255) {
                                char js_path[512] = {0};
                                char rel_path[256] = {0};
                                strncpy(rel_path, src_start, src_len);
                                
                                /* Resolve path */
                                if (rel_path[0] != '/' && strstr(rel_path, "://") == NULL) {
                                    snprintf(js_path, sizeof(js_path), "/hd0/www/%s", rel_path);
                                } else if (strstr(rel_path, "://") == NULL) {
                                    strncpy(js_path, rel_path, sizeof(js_path) - 1);
                                } else {
                                    /* Remote JS — skip for now */
                                    kprintf("[Browser] Skipping remote JS: %s\n", rel_path);
                                    p = tag_end + 1;
                                    continue;
                                }
                                
                                /* Load from VFS */
                                struct vfs_node *js_file = vfs_open(js_path);
                                if (js_file && js_file->size > 0) {
                                    char *js_buf = kcalloc(1, js_file->size + 1);
                                    int js_read = vfs_read(js_file, js_buf, js_file->size, 0);
                                    if (js_read > 0) {
                                        kprintf("[Browser] Running external JS: %s (%d bytes)\n", js_path, js_read);
                                        js_eval(ctx->js_ctx, js_buf);
                                    }
                                    kfree(js_buf);
                                } else {
                                    kprintf("[Browser] JS file not found: %s\n", js_path);
                                }
                            }
                        }
                    }
                    
                    p = tag_end + 1; /* move past '>' */
                    
                    if (!has_src) {
                        /* Inline script */
                        const char *script_end = strstr(p, "</script>");
                        if (script_end) {
                            int script_len = (int)(script_end - p);
                            if (script_len > 0 && script_len < 8192) {
                                char *script_buf = kcalloc(1, script_len + 1);
                                strncpy(script_buf, p, script_len);
                                kprintf("[Browser] Running inline JS (%d bytes)\n", script_len);
                                js_eval(ctx->js_ctx, script_buf);
                                kfree(script_buf);
                            }
                            p = script_end + 9;
                        }
                    } else {
                        /* Skip to end of script tag */
                        const char *script_end = strstr(p, "</script>");
                        if (script_end) p = script_end + 9;
                    }
                }
            }
            
        } else {
            kprintf("[Browser] parse_html returned NULL!\n");
        }
    } else {
        kprintf("[Browser] Empty response or error loading %s\n", load_arg->url);
        
        char error_html[1024];
        if (len == -2) {
            strcpy(error_html, "<html><body bgcolor=\"#FFDDDD\"><br><br><h1>DNS Error</h1><p>Could not resolve the domain name. Check your internet connection or the spelling of the URL.</p></body></html>");
        } else if (len == -13 || len == -3) {
            strcpy(error_html, "<html><body bgcolor=\"#FFDDDD\"><br><br><h1>Connection Failed</h1><p>The connection was aborted or timed out. The server might be down or requires a secure connection (HTTPS).</p></body></html>");
        } else if (len == -4) {
            strcpy(error_html, "<html><body bgcolor=\"#FFDDDD\"><br><br><h1>TLS Handshake Failed</h1><p>ZarqaaOS could not establish a secure connection. The server might require unsupported modern cipher suites.</p></body></html>");
        } else {
            strcpy(error_html, "<html><body bgcolor=\"#FFDDDD\"><br><br><h1>Network Error</h1><p>An unknown network error occurred while trying to load the page.</p></body></html>");
        }
        
        tab->document = browser_renderer_parse_html(error_html, strlen(error_html));
        if (tab->document) {
            tab->layout_root = browser_renderer_build_layout(tab->document);
            browser_renderer_compute_layout(tab->layout_root, ctx->sidebar_width + 10, 80, ctx->win->width - ctx->sidebar_width - 30);
        }
    }
    tab->is_loading = false;
    browser_scheduler_invalidate_paint();
    if (ctx->win) ctx->win->needs_redraw = true;

    kfree(load_arg);
}

void browser_render_current(browser_context_t *ctx) {
    if (!ctx || ctx->tab_count == 0) return;
    
    // Ensure parser is initialized!
    extern void browser_renderer_init(void);
    browser_renderer_init();
    
    browser_tab_t *tab = &ctx->tabs[ctx->active_tab_index];
    
    char full_url[MAX_URL_LEN];
    if (tab->history_count > 0) {
        strncpy(full_url, tab->history[tab->history_index].url, MAX_URL_LEN - 1);
        full_url[MAX_URL_LEN - 1] = '\0';
    } else {
        strcpy(full_url, "zarqaa://start");
    }
    
    tab->is_loading = true;
    
    if (tab->layout_root) {
        browser_renderer_free_layout((render_node_t *)tab->layout_root);
        tab->layout_root = NULL;
    }
    if (tab->document) {
        browser_renderer_destroy_document(tab->document);
        tab->document = NULL;
    }
    
    parsed_url_t parsed;
    if (!url_parse(full_url, &parsed)) {
        kprintf("[Browser] Invalid URL format: %s\n", full_url);
        tab->is_loading = false;
        return;
    }
    
    if (parsed.type == URL_TYPE_HTTP || parsed.type == URL_TYPE_HTTPS) {
        if (!browser_network_ready) {
            strncpy(browser_pending_url, parsed.full_url, MAX_URL_LEN);
            kprintf("[Browser] Network not ready, queueing URL: %s\n", parsed.full_url);
            
            char buf[256];
            strcpy(buf, "<html><body><br><br><br><h2 style='color:#3a86ff'>Waiting for Network...</h2><p>Acquiring IP address via DHCP...</p></body></html>");
            tab->document = browser_renderer_parse_html(buf, strlen(buf));
            if (tab->document) tab->layout_root = browser_renderer_build_layout(tab->document);
            tab->is_loading = false;
            browser_scheduler_invalidate_layout();
            browser_scheduler_invalidate_paint();
            if (ctx->win) ctx->win->needs_redraw = true;
            return;
        }
    }
    
    // URL_TYPE_INTERNAL is exclusively for internal pages (like zarqaa://start)
    if (parsed.type == URL_TYPE_INTERNAL) {
        if (strcmp(parsed.path, "start") == 0) {
            ctx->url_bar_text[0] = '\0';
            ctx->url_pos = 0;
            char buf[512];
            strcpy(buf, "<html><body><br><br><br><h1> N E X U S </h1><p>Welcome to a New Space.</p><p>Type a URL in the omnibox above to begin.</p></body></html>");
            tab->document = browser_renderer_parse_html(buf, strlen(buf));
            if (tab->document) tab->layout_root = browser_renderer_build_layout(tab->document);
            tab->is_loading = false;
            browser_scheduler_invalidate_layout();
            browser_scheduler_invalidate_paint();
            if (ctx->win) ctx->win->needs_redraw = true;
            return;
        } else {
            // Unrecognized internal page
            char buf[128];
            snprintf(buf, 128, "<html><body><h1>Error</h1><p>Internal page zarqaa://%s not found.</p></body></html>", parsed.path);
            tab->document = browser_renderer_parse_html(buf, strlen(buf));
            if (tab->document) tab->layout_root = browser_renderer_build_layout(tab->document);
            tab->is_loading = false;
            browser_scheduler_invalidate_layout();
            browser_scheduler_invalidate_paint();
            if (ctx->win) ctx->win->needs_redraw = true;
            return;
        }
    }
    
    browser_load_arg_t *load_arg = kmalloc(sizeof(browser_load_arg_t));
    load_arg->ctx = ctx;
    load_arg->tab = tab;
    strncpy(load_arg->url, full_url, MAX_URL_LEN);
    
    resource_load(&parsed, browser_resource_loaded_cb, load_arg);
}

void browser_load_url(browser_context_t *ctx, const char *url) {
    if (!ctx || ctx->tab_count == 0) return;
    browser_tab_t *tab = &ctx->tabs[ctx->active_tab_index];
    
    if (tab->is_loading) {
        kprintf("[Browser] Ignoring load request for '%s', already loading.\n", url);
        return;
    }
    
    kprintf("[Browser] Loading URL requested: '%s'\n", url);
    
    char full_url[MAX_URL_LEN];
    
    // Resolve relative URL if it starts with '/'
    if (strstr(url, "://") == NULL && url[0] == '/') {
        if (tab->history_count > 0) {
            const char *base = tab->history[tab->history_index].url;
            parsed_url_t parsed_base;
            if (url_parse(base, &parsed_base) && (parsed_base.type == URL_TYPE_HTTP || parsed_base.type == URL_TYPE_HTTPS)) {
                snprintf(full_url, MAX_URL_LEN, "%s://%s%s", parsed_base.scheme, parsed_base.host, url);
            } else {
                strncpy(full_url, url, MAX_URL_LEN - 1);
                full_url[MAX_URL_LEN - 1] = '\0';
            }
        } else {
            strncpy(full_url, url, MAX_URL_LEN - 1);
            full_url[MAX_URL_LEN - 1] = '\0';
        }
    } else {
        strncpy(full_url, url, MAX_URL_LEN - 1);
        full_url[MAX_URL_LEN - 1] = '\0';
    }
    
    // Simple URL parser
    parsed_url_t parsed;
    if (!url_parse(full_url, &parsed)) {
        // Leave full_url as is if parsing totally fails (which shouldn't happen usually)
    } else {
        strncpy(full_url, parsed.full_url, MAX_URL_LEN - 1);
        full_url[MAX_URL_LEN - 1] = '\0';
    }
    
    // Truncate forward history if we are navigating back then clicking a new link
    if (tab->history_index < tab->history_count - 1) {
        tab->history_count = tab->history_index + 1;
    }
    
    // Add to History stack
    if (tab->history_count < MAX_HISTORY) {
        tab->history_index++;
        strncpy(tab->history[tab->history_index].url, full_url, MAX_URL_LEN - 1);
        
        // Generate a clean title
        char clean_title[100] = {0};
        if (parsed.type == URL_TYPE_INTERNAL && strcmp(parsed.path, "start") == 0) {
            strcpy(clean_title, "New Space");
        } else {
            strncpy(clean_title, parsed.host[0] != '\0' ? parsed.host : parsed.path, 99);
        }
        
        if (strlen(clean_title) == 0) {
            strcpy(clean_title, "New Space");
        }
        
        strncpy(tab->history[tab->history_index].title, clean_title, 99);
        tab->history_count++;
    }
    
    // Update the UI omnibox to reflect the loaded URL
    strncpy(ctx->url_bar_text, full_url, 255);
    ctx->url_bar_text[255] = '\0';
    ctx->url_pos = strlen(ctx->url_bar_text);
    
    tab->redirects = 0;
    browser_render_current(ctx);
}

void browser_toggle_bookmark(browser_context_t *ctx) {
    if (!ctx || ctx->tab_count == 0) return;
    browser_tab_t *tab = &ctx->tabs[ctx->active_tab_index];
    if (tab->history_count == 0) return;
    
    char* current_url = tab->history[tab->history_index].url;
    
    // Check if it exists
    for (int i = 0; i < ctx->bookmark_count; i++) {
        if (strcmp(ctx->bookmarks[i].url, current_url) == 0) {
            // Remove it
            for (int j = i; j < ctx->bookmark_count - 1; j++) {
                ctx->bookmarks[j] = ctx->bookmarks[j+1];
            }
            ctx->bookmark_count--;
            ctx->win->needs_redraw = true;
            return;
        }
    }
    
    // Add it
    if (ctx->bookmark_count < MAX_BOOKMARKS) {
        strncpy(ctx->bookmarks[ctx->bookmark_count].url, current_url, MAX_URL_LEN - 1);
        strncpy(ctx->bookmarks[ctx->bookmark_count].title, tab->history[tab->history_index].title, 99);
        ctx->bookmark_count++;
        ctx->win->needs_redraw = true;
    }
}

void browser_cleanup(struct window *win) {
    browser_context_t *ctx = (browser_context_t *)win->user_data;
    if (ctx) {
        for (int i = 0; i < ctx->tab_count; i++) {
            browser_tab_t *curr = &ctx->tabs[i];
            if (curr->layout_root) browser_renderer_free_layout((render_node_t *)curr->layout_root);
            if (curr->document) browser_renderer_destroy_document(curr->document);
        }
        if (ctx->canvas) {
            // Note: target_surface is NOT freed here because canvas_destroy takes care of freeing
            // the target_surface if it was allocated, per ZarqaaOS canvas ownership model.
            // If the model changes, uncomment kfree(ctx->canvas->target_surface);
            canvas_destroy(ctx->canvas);
        }
        if (ctx->back_buffer) {
            kfree(ctx->back_buffer);
        }
        kfree(ctx);
    }
}
