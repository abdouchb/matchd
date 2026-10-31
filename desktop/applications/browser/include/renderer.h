#ifndef ZARQAA_BROWSER_RENDERER_H
#define ZARQAA_BROWSER_RENDERER_H

#include "zarqaa/types.h"
#include "gui/surface.h"

// Lexbor integration
void browser_renderer_init(void);
void *browser_renderer_parse_html(const char *html_data, size_t length);
void browser_renderer_destroy_document(void *document);

// Layout structure (Opaque pointer to avoid exposing Lexbor everywhere)
typedef struct render_node render_node_t;

render_node_t *browser_renderer_build_layout(void *document);
void browser_renderer_compute_layout(render_node_t *root, int x, int y, int width);
void browser_renderer_paint(surface_t *surf, render_node_t *root, int scroll_y, int clip_y, int clip_h);
void browser_renderer_free_layout(render_node_t *root);
int browser_renderer_get_document_height(render_node_t *root);

render_node_t *browser_renderer_get_node_at(render_node_t *root, int x, int y);
const char *browser_renderer_get_href(render_node_t *node);
void browser_renderer_extract_links(render_node_t *node, char *buffer, int max_len);

#endif
