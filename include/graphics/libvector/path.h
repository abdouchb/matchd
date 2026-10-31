#pragma once

#include "graphics/libvector/vector.h"
#include "graphics/libvector/bezier.h"

typedef enum {
    PATH_CMD_MOVE_TO,
    PATH_CMD_LINE_TO,
    PATH_CMD_QUAD_TO,
    PATH_CMD_CUBIC_TO,
    PATH_CMD_CLOSE
} path_cmd_t;

typedef struct {
    path_cmd_t *cmds;
    vec2_t *pts;
    int num_cmds;
    int num_pts;
    int max_cmds;
    int max_pts;
} path_t;

// Path construction
path_t* path_create(void);
void path_destroy(path_t *path);
void path_clear(path_t *path);

void path_move_to(path_t *path, float x, float y);
void path_line_to(path_t *path, float x, float y);
void path_quad_to(path_t *path, float cx, float cy, float x, float y);
void path_cubic_to(path_t *path, float c1x, float c1y, float c2x, float c2y, float x, float y);
void path_close(path_t *path);

// Convert all curves into line segments using a specific tolerance
// Returns a new path containing only MOVE_TO, LINE_TO, and CLOSE
path_t* path_flatten(const path_t *path, float tolerance);
