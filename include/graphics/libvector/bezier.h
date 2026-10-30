#pragma once

#include "graphics/libvector/vector.h"

// Flatten quadratic bezier curve into a series of line segments
// p0: start, p1: control, p2: end
// tolerance: maximum error distance
// out_points: array to store result
// max_points: size of out_points
// returns number of points generated
int bezier_quad_flatten(vec2_t p0, vec2_t p1, vec2_t p2, float tolerance, vec2_t *out_points, int max_points);

// Flatten cubic bezier curve into a series of line segments
// p0: start, p1: control 1, p2: control 2, p3: end
int bezier_cubic_flatten(vec2_t p0, vec2_t p1, vec2_t p2, vec2_t p3, float tolerance, vec2_t *out_points, int max_points);
