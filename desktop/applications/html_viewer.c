/*
 * Zarqaa Vector Browser (ZVB) - Phase 4
 * Creator: Abdelhak Chabbi
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "filesystem/vfs.h"
#include "libhtml/parser.h"
#include "libhtml/scene_builder.h"
#include "graphics/libvector/scene_graph.h"
#include "graphics/libvector/shape.h"
#include "zarqaa/string.h"
#include "kernel/scheduler.h"
#include "net/http.h"
#include "net/url.h"

struct html_viewer_state {
    struct window *win;
    bool is_loading;
    
    dom_node_t *dom_root;
    scene_node_t *scene_root;
    
    char url_bar[256];
    int url_pos;
    
    char history[10][256];
    int history_idx;
    int history_count;
    
    bool show_context_menu;
    int ctx_x;
    int ctx_y;
    scene_node_t *context_menu_root;
};

// Creates a vector glassmorphic context menu
static scene_node_t* build_context_menu(int x, int y) {
    scene_node_t *menu = scene_node_create();
    transform_translate(&menu->local_transform, (float)x, (float)y);
    
    menu->path = path_create();
    shape_add_round_rect(menu->path, 0, 0, 180, 80, 8);
    menu->has_fill = true;
    menu->fill_color = (color_f_t){0.08f, 0.12f, 0.19f, 0.95f}; // Dark glass #141F30
    menu->has_stroke = true;
    menu->stroke_color = (color_f_t){0.14f, 0.23f, 0.33f, 1.0f}; // Border
    menu->stroke_width = 1.5f;
    
    return menu;
}

static void my_http_callback(const char *html_data, int length, int status_code, void *arg) {
    struct html_viewer_state *state = (struct html_viewer_state *)arg;
    
    if (html_data && length > 0 && (status_code == 200 || status_code == 0)) {
        state->dom_root = html_parse((char *)html_data);
        if (state->dom_root) {
            state->scene_root = dom_to_scene_graph(state->dom_root);
        }
    } else {
        const char *err_html = "<svg><rect style=\"width:600; height:450; background-color:#1E1E2E;\" /><circle cx=\"300\" cy=\"225\" r=\"50\" style=\"background-color:#F38BA8;\" /><rect style=\"left:250; top:200; width:100; height:50; background-color:transparent;\" href=\"/hd0/www/index.html\" /></svg>";
        state->dom_root = html_parse((char*)err_html);
        if (state->dom_root) state->scene_root = dom_to_scene_graph(state->dom_root);
    }
    
    state->is_loading = false;
    if (state->win) state->win->needs_redraw = true;
}

static void load_url(struct html_viewer_state *state, bool push_history) {
    if (push_history) {
        if (state->history_idx < 9) {
            state->history_idx++;
        } else {
            // Shift history left
            for (int i = 0; i < 9; i++) {
                strcpy(state->history[i], state->history[i+1]);
            }
        }
        strcpy(state->history[state->history_idx], state->url_bar);
        state->history_count = state->history_idx + 1;
    }

    scene_node_t *old_scene = state->scene_root;
    dom_node_t *old_dom = state->dom_root;
    state->scene_root = NULL;
    state->dom_root = NULL;
    
    if (old_scene) scene_node_destroy(old_scene);
    if (old_dom) dom_free(old_dom);
    
    if (strncmp(state->url_bar, "http://", 7) == 0 || strncmp(state->url_bar, "https://", 8) == 0) {
        parsed_url_t parsed;
        if (url_parse(state->url_bar, &parsed)) {
            http_get(&parsed, my_http_callback, state);
        } else {
            const char *err_html = "<svg><rect style=\"width:600; height:450; background-color:#1E1E2E;\" /><text x=\"200\" y=\"225\" style=\"fill:#FFF;\">Invalid URL!</text></svg>";
            state->dom_root = html_parse((char*)err_html);
            if (state->dom_root) state->scene_root = dom_to_scene_graph(state->dom_root);
            state->is_loading = false;
            if (state->win) state->win->needs_redraw = true;
        }
    } else {
        // Read from VFS
        const char *vfs_path = state->url_bar;
        if (strncmp(vfs_path, "file://", 7) == 0) {
            vfs_path += 7;
        }
        struct vfs_node *file = vfs_open(vfs_path);
        if (file && file->type == VFS_FILE && file->size > 0) {
            char *buffer = kcalloc(1, file->size + 1);
            int read_bytes = vfs_read(file, buffer, file->size, 0);
            if (read_bytes > 0) {
                buffer[read_bytes] = '\0';
                state->dom_root = html_parse(buffer);
                if (state->dom_root) {
                    state->scene_root = dom_to_scene_graph(state->dom_root);
                }
            }
            kfree(buffer);
        } else {
            // Fallback Error UI
            const char *err_html = "<svg><rect style=\"width:600; height:450; background-color:#1E1E2E;\" /><circle cx=\"300\" cy=\"225\" r=\"50\" style=\"background-color:#F38BA8;\" /><rect style=\"left:250; top:200; width:100; height:50; background-color:transparent;\" href=\"/hd0/www/index.html\" /></svg>";
            state->dom_root = html_parse(err_html);
            if (state->dom_root) {
                state->scene_root = dom_to_scene_graph(state->dom_root);
            }
        }
        state->is_loading = false;
        if (state->win) state->win->needs_redraw = true;
    }
}

struct html_load_req {
    struct html_viewer_state *state;
    bool push_history;
};

static void html_loader_task(void *arg) {
    struct html_load_req *req = (struct html_load_req *)arg;
    struct html_viewer_state *state = req->state;
    
    load_url(state, req->push_history);
    
    kfree(req);
    task_exit();
}

static void queue_load(struct html_viewer_state *state, bool push_history) {
    if (state->is_loading) return;
    state->is_loading = true;
    if (state->win) state->win->needs_redraw = true;
    
    struct html_load_req *req = kmalloc(sizeof(struct html_load_req));
    req->state = state;
    req->push_history = push_history;
    
    task_create(html_loader_task, req, "html_loader");
}

static dom_node_t* find_link_at(dom_node_t *node, int mx, int my) {
    if (!node) return NULL;
    
    if (node->href[0] != '\0') {
        bool hit = false;
        if (node->type == TAG_RECT) {
            if (mx >= node->style.x && mx <= node->style.x + node->style.width &&
                my >= node->style.y && my <= node->style.y + node->style.height) hit = true;
        } else if (node->type == TAG_CIRCLE) {
            int dx = mx - node->cx;
            int dy = my - node->cy;
            if (dx*dx + dy*dy <= node->r * node->r) hit = true;
        }
        if (hit) return node;
    }
    
    for (int i = 0; i < node->child_count; i++) {
        dom_node_t *res = find_link_at(node->children[i], mx, my);
        if (res) return res;
    }
    return NULL;
}

static void html_viewer_on_draw(struct window *win) {
    struct html_viewer_state *state = (struct html_viewer_state *)win->user_data;
    
    // Clear background
    fb_fill_rect(0, 0, win->width, win->height, 0xFF0A0F1A);
    
    if (state->is_loading) {
        fb_draw_string_transparent(win->width/2 - 60, win->height/2, "Loading Webpage...", 0xFFFFFFFF);
    } else {
        // 1. Draw the actual webpage content
        if (state->scene_root) {
            transform_t offset;
            transform_identity(&offset);
            transform_translate(&offset, 0, 60);
            scene_node_update_transforms(state->scene_root, &offset);
            scene_graph_render(state->scene_root);
        }
    }
    
    // 2. Draw Native Navigation Bar
    gfx_draw_glass_panel(0, 0, win->width, 60, 0, 0xDD1E1E2E, 0xFF333344, 255);
    
    // Draw Back Button (circle at 35, 30, r=15)
    bool can_back = state->history_idx > 0;
    gfx_draw_circle_filled(35, 30, 15, can_back ? 0xFF313244 : 0xFF181825);
    if (can_back) {
        gfx_draw_line_aa(38, 23, 31, 30, 0xFFCDD6F4);
        gfx_draw_line_aa(31, 30, 38, 37, 0xFFCDD6F4);
        gfx_draw_line_aa(31, 30, 41, 30, 0xFFCDD6F4);
    }
    
    // Draw Forward Button (circle at 75, 30, r=15)
    bool can_fwd = state->history_idx < state->history_count - 1;
    gfx_draw_circle_filled(75, 30, 15, can_fwd ? 0xFF313244 : 0xFF181825);
    if (can_fwd) {
        gfx_draw_line_aa(72, 23, 79, 30, 0xFFCDD6F4);
        gfx_draw_line_aa(79, 30, 72, 37, 0xFFCDD6F4);
        gfx_draw_line_aa(79, 30, 69, 30, 0xFFCDD6F4);
    }
    
    // URL Bar
    gfx_draw_rounded_rect_filled(100, 15, win->width - 120, 30, 15, 0xFF11111B);
    gfx_draw_rounded_rect_aa(100, 15, win->width - 120, 30, 15, 0xFF45475A);
    
    // URL Text
    char display_url[256];
    strcpy(display_url, state->url_bar);
    if (win->focused) strcat(display_url, "_");
    fb_draw_string_transparent(115, 22, display_url, 0xFFCDD6F4);
    
    // 3. Draw Context Menu Overlay
    if (state->show_context_menu && state->context_menu_root) {
        scene_node_update_transforms(state->context_menu_root, NULL);
        scene_graph_render(state->context_menu_root);
        
        // Context Menu Text
        fb_draw_string_transparent(state->ctx_x + 15, state->ctx_y + 15, "1. Open Local Site", 0xFF00E5FF);
        fb_draw_string_transparent(state->ctx_x + 15, state->ctx_y + 45, "2. Refresh VFS", 0xFFFFFFFF);
    }
}

static void html_viewer_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    struct html_viewer_state *state = (struct html_viewer_state *)win->user_data;
    
    if (right) {
        state->show_context_menu = true;
        state->ctx_x = mx;
        state->ctx_y = my;
        
        if (state->context_menu_root) scene_node_destroy(state->context_menu_root);
        state->context_menu_root = build_context_menu(mx, my);
        win->needs_redraw = true;
        return;
    }
    
    if (left) {
        if (state->show_context_menu) {
            // Check if clicked inside menu
            if (mx >= state->ctx_x && mx <= state->ctx_x + 180 &&
                my >= state->ctx_y && my <= state->ctx_y + 80) {
                
                if (my < state->ctx_y + 40) {
                    // Option 1: Open Local Site
                    strcpy(state->url_bar, "/hd0/www/index.html");
                    state->url_pos = strlen(state->url_bar);
                    queue_load(state, true);
                } else {
                    // Option 2: Refresh
                    queue_load(state, false);
                }
            }
            state->show_context_menu = false;
            win->needs_redraw = true;
            return;
        }
        
        // Check Back/Forward Orbs
        int dx_back = mx - 35; int dy_back = my - 30;
        if (dx_back*dx_back + dy_back*dy_back <= 225) {
            if (state->history_idx > 0 && !state->is_loading) {
                state->history_idx--;
                strcpy(state->url_bar, state->history[state->history_idx]);
                state->url_pos = strlen(state->url_bar);
                queue_load(state, false);
            }
            return;
        }
        
        int dx_fwd = mx - 75; int dy_fwd = my - 30;
        if (dx_fwd*dx_fwd + dy_fwd*dy_fwd <= 225) {
            if (state->history_idx < state->history_count - 1 && !state->is_loading) {
                state->history_idx++;
                strcpy(state->url_bar, state->history[state->history_idx]);
                state->url_pos = strlen(state->url_bar);
                queue_load(state, false);
            }
            return;
        }
        
        // Check HTML links
        if (!state->is_loading) {
            dom_node_t *clicked_link = find_link_at(state->dom_root, mx, my - 60);
            if (clicked_link) {
                strcpy(state->url_bar, clicked_link->href);
                state->url_pos = strlen(state->url_bar);
                queue_load(state, true);
                return;
            }
        }
    }
}

static void html_viewer_on_key(struct window *win, char key, uint8_t scancode) {
    (void)scancode;
    struct html_viewer_state *state = (struct html_viewer_state *)win->user_data;
    
    if (key == '\b') {
        if (state->url_pos > 0) {
            state->url_bar[--state->url_pos] = '\0';
        }
    } else if (key == '\n') {
        queue_load(state, true);
    } else if (key >= 32 && key <= 126) {
        if (state->url_pos < 250) {
            state->url_bar[state->url_pos++] = key;
            state->url_bar[state->url_pos] = '\0';
        }
    }
    win->needs_redraw = true;
}

static void html_viewer_on_close(struct window *win) {
    struct html_viewer_state *state = (struct html_viewer_state *)win->user_data;
    if (state) {
        if (state->scene_root) scene_node_destroy(state->scene_root);
        if (state->dom_root) dom_free(state->dom_root);
        if (state->context_menu_root) scene_node_destroy(state->context_menu_root);
        kfree(state);
    }
}

struct window *browser_open_url(const char *url) {
    struct window *win = wm_create_window("Zarqaa Vector Browser", 150, 100, 600, 450);
    if (!win) return NULL;

    struct html_viewer_state *state = kcalloc(1, sizeof(struct html_viewer_state));
    
    if (url && url[0] != '\0') {
        strncpy(state->url_bar, url, sizeof(state->url_bar) - 1);
    } else {
        strcpy(state->url_bar, "http://10.0.2.2:8000/");
    }
    state->url_pos = strlen(state->url_bar);
    state->show_context_menu = false;
    
    state->history_idx = -1;
    state->history_count = 0;
    
    state->win = win;
    
    // Initial Load Async
    queue_load(state, true);
    
    win->user_data = state;
    win->on_draw = html_viewer_on_draw;
    win->on_mouse = html_viewer_on_mouse;
    win->on_key = html_viewer_on_key;
    win->on_close = html_viewer_on_close;
    
    return win;
}

struct window *html_viewer_open(void) {
    return browser_open_url(NULL);
}
