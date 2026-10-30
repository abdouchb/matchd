#include "graphics/libvector/shape.h"

void shape_add_rect(path_t *path, float x, float y, float w, float h) {
    path_move_to(path, x, y);
    path_line_to(path, x + w, y);
    path_line_to(path, x + w, y + h);
    path_line_to(path, x, y + h);
    path_close(path);
}

void shape_add_round_rect(path_t *path, float x, float y, float w, float h, float radius) {
    // Top-left
    path_move_to(path, x + radius, y);
    // Top edge
    path_line_to(path, x + w - radius, y);
    // Top-right corner
    path_quad_to(path, x + w, y, x + w, y + radius);
    // Right edge
    path_line_to(path, x + w, y + h - radius);
    // Bottom-right corner
    path_quad_to(path, x + w, y + h, x + w - radius, y + h);
    // Bottom edge
    path_line_to(path, x + radius, y + h);
    // Bottom-left corner
    path_quad_to(path, x, y + h, x, y + h - radius);
    // Left edge
    path_line_to(path, x, y + radius);
    // Top-left corner
    path_quad_to(path, x, y, x + radius, y);
    path_close(path);
}

void shape_add_circle(path_t *path, float cx, float cy, float r) {
    shape_add_ellipse(path, cx, cy, r, r);
}

void shape_add_ellipse(path_t *path, float cx, float cy, float rx, float ry) {
    // Magic number for approximating a circle with cubic beziers
    float k = 0.552284749831f; 
    float kx = k * rx;
    float ky = k * ry;
    
    path_move_to(path, cx, cy - ry);
    
    // Top right
    path_cubic_to(path, cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
    // Bottom right
    path_cubic_to(path, cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
    // Bottom left
    path_cubic_to(path, cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
    // Top left
    path_cubic_to(path, cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
    
    path_close(path);
}
