#include "graphics/libvector/rasterizer.h"
#include "kernel/heap.h"
#include "drivers/framebuffer.h"

typedef struct {
    int y_min;
    int y_max;
    float x; // current x
    float dx; // 1 / slope
} edge_t;

// A simple sort for edges
static void sort_edges_by_y(edge_t *edges, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (edges[i].y_min > edges[j].y_min) {
                edge_t temp = edges[i];
                edges[i] = edges[j];
                edges[j] = temp;
            }
        }
    }
}

static void sort_aet_by_x(edge_t **aet, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (aet[i]->x > aet[j]->x) {
                edge_t *temp = aet[i];
                aet[i] = aet[j];
                aet[j] = temp;
            }
        }
    }
}

void rasterize_path_fill(const path_t *path, const transform_t *transform, color_f_t fill_color) {
    if (!path || path->num_cmds == 0) return;
    
    // 1. Flatten path
    path_t *flat = path_flatten(path, 0.5f);
    if (!flat) return;
    
    // 2. Transform points if needed
    if (transform) {
        for (int i = 0; i < flat->num_pts; i++) {
            flat->pts[i] = transform_point(transform, flat->pts[i]);
        }
    }
    
    // 3. Build edge list
    int max_edges = flat->num_pts;
    edge_t *edges = kmalloc(max_edges * sizeof(edge_t));
    int edge_count = 0;
    
    vec2_t p0 = {0,0};
    vec2_t first_pt = {0,0};
    int pt_idx = 0;
    
    for (int i = 0; i < flat->num_cmds; i++) {
        switch (flat->cmds[i]) {
            case PATH_CMD_MOVE_TO:
                p0 = flat->pts[pt_idx++];
                first_pt = p0;
                break;
            case PATH_CMD_LINE_TO: {
                vec2_t p1 = flat->pts[pt_idx++];
                
                int y0 = (int)(p0.y + 0.5f);
                int y1 = (int)(p1.y + 0.5f);
                
                if (y0 != y1) {
                    edge_t e;
                    if (y0 < y1) {
                        e.y_min = y0;
                        e.y_max = y1;
                        e.x = p0.x;
                        e.dx = (p1.x - p0.x) / (float)(y1 - y0);
                    } else {
                        e.y_min = y1;
                        e.y_max = y0;
                        e.x = p1.x;
                        e.dx = (p0.x - p1.x) / (float)(y0 - y1);
                    }
                    edges[edge_count++] = e;
                }
                p0 = p1;
                break;
            }
            case PATH_CMD_CLOSE: {
                vec2_t p1 = first_pt;
                int y0 = (int)(p0.y + 0.5f);
                int y1 = (int)(p1.y + 0.5f);
                
                if (y0 != y1) {
                    edge_t e;
                    if (y0 < y1) {
                        e.y_min = y0;
                        e.y_max = y1;
                        e.x = p0.x;
                        e.dx = (p1.x - p0.x) / (float)(y1 - y0);
                    } else {
                        e.y_min = y1;
                        e.y_max = y0;
                        e.x = p1.x;
                        e.dx = (p0.x - p1.x) / (float)(y0 - y1);
                    }
                    edges[edge_count++] = e;
                }
                p0 = p1;
                break;
            }
            default: break;
        }
    }
    
    if (edge_count == 0) {
        kfree(edges);
        path_destroy(flat);
        return;
    }
    
    // 4. Sort edges by Y
    sort_edges_by_y(edges, edge_count);
    
    int min_y = edges[0].y_min;
    int max_y = edges[0].y_max;
    for (int i = 1; i < edge_count; i++) {
        if (edges[i].y_max > max_y) max_y = edges[i].y_max;
    }
    
    // Active Edge Table
    edge_t **aet = kmalloc(edge_count * sizeof(edge_t*));
    int aet_count = 0;
    int next_edge = 0;
    
    uint32_t c32 = color_f_to_u32(fill_color);
    
    // 5. Scanline fill
    for (int y = min_y; y < max_y; y++) {
        // Add new edges
        while (next_edge < edge_count && edges[next_edge].y_min == y) {
            aet[aet_count++] = &edges[next_edge++];
        }
        
        // Remove old edges
        int out = 0;
        for (int i = 0; i < aet_count; i++) {
            if (aet[i]->y_max > y) {
                aet[out++] = aet[i];
            }
        }
        aet_count = out;
        
        // Sort AET by X
        sort_aet_by_x(aet, aet_count);
        
        // Fill between pairs
        for (int i = 0; i < aet_count - 1; i += 2) {
            int x_start = (int)(aet[i]->x + 0.5f);
            int x_end = (int)(aet[i+1]->x + 0.5f);
            fb_fill_rect(x_start, y, x_end - x_start, 1, c32);
        }
        
        // Update X
        for (int i = 0; i < aet_count; i++) {
            aet[i]->x += aet[i]->dx;
        }
    }
    
    kfree(aet);
    kfree(edges);
    path_destroy(flat);
}

static void draw_thick_line(int x0, int y0, int x1, int y1, uint32_t color, float thickness) {
    if (thickness <= 1.0f) {
        fb_draw_line(x0, y0, x1, y1, color);
        return;
    }
    
    int thick = (int)(thickness + 0.5f);
    int dx = x1 - x0;
    int dy = y1 - y0;
    int abs_dx = dx > 0 ? dx : -dx;
    int abs_dy = dy > 0 ? dy : -dy;
    
    int half_thick = thick / 2;
    
    if (abs_dx >= abs_dy) {
        for (int i = -half_thick; i <= half_thick; i++) {
            fb_draw_line(x0, y0 + i, x1, y1 + i, color);
        }
    } else {
        for (int i = -half_thick; i <= half_thick; i++) {
            fb_draw_line(x0 + i, y0, x1 + i, y1, color);
        }
    }
}

void rasterize_path_stroke(const path_t *path, const transform_t *transform, color_f_t stroke_color, float thickness) {
    // Basic fallback: just draw lines using bresenham or a thick line
    // A proper stroke expands the path and fills it, but we'll do simple lines for now
    if (!path || path->num_cmds == 0) return;
    
    path_t *flat = path_flatten(path, 0.5f);
    if (!flat) return;
    
    if (transform) {
        for (int i = 0; i < flat->num_pts; i++) {
            flat->pts[i] = transform_point(transform, flat->pts[i]);
        }
    }
    
    uint32_t c32 = color_f_to_u32(stroke_color);
    vec2_t p0 = {0,0};
    vec2_t first_pt = {0,0};
    int pt_idx = 0;
    
    for (int i = 0; i < flat->num_cmds; i++) {
        switch (flat->cmds[i]) {
            case PATH_CMD_MOVE_TO:
                p0 = flat->pts[pt_idx++];
                first_pt = p0;
                break;
            case PATH_CMD_LINE_TO: {
                vec2_t p1 = flat->pts[pt_idx++];
                draw_thick_line((int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, c32, thickness);
                p0 = p1;
                break;
            }
            case PATH_CMD_CLOSE: {
                draw_thick_line((int)p0.x, (int)p0.y, (int)first_pt.x, (int)first_pt.y, c32, thickness);
                p0 = first_pt;
                break;
            }
            default: break;
        }
    }
    
    path_destroy(flat);
}
