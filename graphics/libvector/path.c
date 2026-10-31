#include "graphics/libvector/path.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

#define INITIAL_CAPACITY 32

path_t* path_create(void) {
    path_t *p = kmalloc(sizeof(path_t));
    if (!p) return NULL;
    
    p->max_cmds = INITIAL_CAPACITY;
    p->max_pts = INITIAL_CAPACITY * 2;
    p->cmds = kmalloc(p->max_cmds * sizeof(path_cmd_t));
    p->pts = kmalloc(p->max_pts * sizeof(vec2_t));
    p->num_cmds = 0;
    p->num_pts = 0;
    
    return p;
}

void path_destroy(path_t *path) {
    if (!path) return;
    kfree(path->cmds);
    kfree(path->pts);
    kfree(path);
}

void path_clear(path_t *path) {
    if (!path) return;
    path->num_cmds = 0;
    path->num_pts = 0;
}

static void ensure_capacity(path_t *path, int cmds_needed, int pts_needed) {
    if (path->num_cmds + cmds_needed > path->max_cmds) {
        int new_max = path->max_cmds * 2;
        path_cmd_t *new_cmds = kmalloc(new_max * sizeof(path_cmd_t));
        memcpy(new_cmds, path->cmds, path->num_cmds * sizeof(path_cmd_t));
        kfree(path->cmds);
        path->cmds = new_cmds;
        path->max_cmds = new_max;
    }
    
    if (path->num_pts + pts_needed > path->max_pts) {
        int new_max = path->max_pts * 2;
        if (new_max < path->num_pts + pts_needed) {
            new_max = path->num_pts + pts_needed + INITIAL_CAPACITY;
        }
        vec2_t *new_pts = kmalloc(new_max * sizeof(vec2_t));
        memcpy(new_pts, path->pts, path->num_pts * sizeof(vec2_t));
        kfree(path->pts);
        path->pts = new_pts;
        path->max_pts = new_max;
    }
}

void path_move_to(path_t *path, float x, float y) {
    ensure_capacity(path, 1, 1);
    path->cmds[path->num_cmds++] = PATH_CMD_MOVE_TO;
    path->pts[path->num_pts++] = (vec2_t){x, y};
}

void path_line_to(path_t *path, float x, float y) {
    ensure_capacity(path, 1, 1);
    path->cmds[path->num_cmds++] = PATH_CMD_LINE_TO;
    path->pts[path->num_pts++] = (vec2_t){x, y};
}

void path_quad_to(path_t *path, float cx, float cy, float x, float y) {
    ensure_capacity(path, 1, 2);
    path->cmds[path->num_cmds++] = PATH_CMD_QUAD_TO;
    path->pts[path->num_pts++] = (vec2_t){cx, cy};
    path->pts[path->num_pts++] = (vec2_t){x, y};
}

void path_cubic_to(path_t *path, float c1x, float c1y, float c2x, float c2y, float x, float y) {
    ensure_capacity(path, 1, 3);
    path->cmds[path->num_cmds++] = PATH_CMD_CUBIC_TO;
    path->pts[path->num_pts++] = (vec2_t){c1x, c1y};
    path->pts[path->num_pts++] = (vec2_t){c2x, c2y};
    path->pts[path->num_pts++] = (vec2_t){x, y};
}

void path_close(path_t *path) {
    ensure_capacity(path, 1, 0);
    path->cmds[path->num_cmds++] = PATH_CMD_CLOSE;
}

path_t* path_flatten(const path_t *path, float tolerance) {
    path_t *flat = path_create();
    if (!flat) return NULL;
    
    vec2_t current_pt = {0, 0};
    int pt_idx = 0;
    vec2_t curve_pts[128]; // Max points per curve flattening
    
    for (int i = 0; i < path->num_cmds; i++) {
        switch (path->cmds[i]) {
            case PATH_CMD_MOVE_TO:
                current_pt = path->pts[pt_idx++];
                path_move_to(flat, current_pt.x, current_pt.y);
                break;
                
            case PATH_CMD_LINE_TO:
                current_pt = path->pts[pt_idx++];
                path_line_to(flat, current_pt.x, current_pt.y);
                break;
                
            case PATH_CMD_QUAD_TO: {
                vec2_t ctrl = path->pts[pt_idx++];
                vec2_t end = path->pts[pt_idx++];
                int count = bezier_quad_flatten(current_pt, ctrl, end, tolerance, curve_pts, 128);
                for (int p = 1; p < count; p++) {
                    path_line_to(flat, curve_pts[p].x, curve_pts[p].y);
                }
                current_pt = end;
                break;
            }
                
            case PATH_CMD_CUBIC_TO: {
                vec2_t ctrl1 = path->pts[pt_idx++];
                vec2_t ctrl2 = path->pts[pt_idx++];
                vec2_t end = path->pts[pt_idx++];
                int count = bezier_cubic_flatten(current_pt, ctrl1, ctrl2, end, tolerance, curve_pts, 128);
                for (int p = 1; p < count; p++) {
                    path_line_to(flat, curve_pts[p].x, curve_pts[p].y);
                }
                current_pt = end;
                break;
            }
                
            case PATH_CMD_CLOSE:
                path_close(flat);
                break;
        }
    }
    
    return flat;
}
