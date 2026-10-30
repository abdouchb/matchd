#pragma once

#include "graphics/libvector/path.h"

// Factory functions to generate paths for standard shapes
void shape_add_rect(path_t *path, float x, float y, float w, float h);
void shape_add_round_rect(path_t *path, float x, float y, float w, float h, float radius);
void shape_add_circle(path_t *path, float cx, float cy, float r);
void shape_add_ellipse(path_t *path, float cx, float cy, float rx, float ry);
