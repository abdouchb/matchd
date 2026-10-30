#pragma once

#include "graphics/libvector/path.h"
#include "graphics/libvector/vector.h"

// Rasterizes a filled path into the current framebuffer render target
// transform: optional transformation matrix applied to path before rasterizing
// fill_color: color to fill the path
void rasterize_path_fill(const path_t *path, const transform_t *transform, color_f_t fill_color);

// Rasterizes the stroke (outline) of a path
// thickness: stroke width
void rasterize_path_stroke(const path_t *path, const transform_t *transform, color_f_t stroke_color, float thickness);
