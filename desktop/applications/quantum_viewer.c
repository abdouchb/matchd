/*
 * Zarqaa OS 0.1 - QuantumPeak OS Viewer
 * Creator: Abdelhak Chabbi
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "libhtml/parser.h"
#include "libhtml/layout.h"
#include "libhtml/render.h"
#include "zarqaa/string.h"

struct quantum_viewer_state {
    dom_node_t *dom;
};

/* Simplified QuantumPeak layout */
static const char *quantum_html = 
"<div style=\"width: 800px; height: 600px; background-color: #070b12; display: flex; flex-direction: column; gap: 0px;\">"
"  <div style=\"width: 800px; height: 40px; background-color: #141f30; display: flex; gap: 20px;\">"
"    <span style=\"color: #80a0c4;\">Search System</span>"
"    <span style=\"color: #00f0ff;\">QuantumPeak OS</span>"
"  </div>"
"  <div style=\"width: 800px; height: 500px; display: flex; gap: 20px;\">"
"    <div style=\"width: 150px; height: 500px; background-color: #141f30;\">"
"      <span style=\"color: #ffffff;\">Home</span>"
"      <span style=\"color: #ffffff;\">Monitor</span>"
"      <span style=\"color: #ffffff;\">Network</span>"
"    </div>"
"    <div style=\"width: 600px; height: 500px; background-color: #0a101d; border-radius: 10px;\">"
"      <circle cx=\"300\" cy=\"250\" r=\"80\" style=\"color: #00f0ff;\"></circle>"
"      <span style=\"color: #ffffff;\">System Score: 95</span>"
"    </div>"
"  </div>"
"  <div style=\"width: 800px; height: 60px; background-color: #141f30; display: flex; gap: 10px;\">"
"    <circle cx=\"40\" cy=\"30\" r=\"20\" style=\"color: #00f0ff;\"></circle>"
"    <circle cx=\"100\" cy=\"30\" r=\"20\" style=\"color: #3a86ff;\"></circle>"
"  </div>"
"</div>";

static void quantum_viewer_on_draw(struct window *win) {
    struct quantum_viewer_state *state = (struct quantum_viewer_state *)win->user_data;
    if (!state->dom) return;
    
    /* 1. Fill window background to black */
    fb_fill_rect(0, 0, win->width, win->height, 0xFF000000);
    
    /* 2. Compute Layout within window bounds */
    layout_compute(state->dom, 0, 0, win->width, win->height);
    
    /* 3. Render directly to the window's local content_buffer */
    state->dom->style.x = 0; /* Window-local coordinates */
    state->dom->style.y = 0;
    
    surface_t win_surf;
    memset(&win_surf, 0, sizeof(surface_t));
    win_surf.width = win->width;
    win_surf.height = win->height;
    win_surf.pitch = win->width * 4;
    win_surf.format = PIXEL_FORMAT_ARGB32;
    win_surf.cpu_pointer = win->content_buffer;
    
    html_render(&win_surf, state->dom);
}

static void quantum_viewer_on_close(struct window *win) {
    struct quantum_viewer_state *state = (struct quantum_viewer_state *)win->user_data;
    if (state) {
        if (state->dom) dom_free(state->dom);
        kfree(state);
    }
}

struct window *quantum_viewer_open(void) {
    struct window *win = wm_create_window("QuantumPeak HTML Engine", 50, 50, 800, 530);
    if (!win) return NULL;

    struct quantum_viewer_state *state = kcalloc(1, sizeof(struct quantum_viewer_state));
    
    /* Parse the HTML once when opening */
    state->dom = html_parse(quantum_html);
    
    win->user_data = state;
    win->on_draw = quantum_viewer_on_draw;
    win->on_close = quantum_viewer_on_close;
    
    return win;
}
