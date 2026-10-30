#pragma once

#include "zarqaa/types.h"

// 2D Point/Vector
typedef struct {
    float x;
    float y;
} vec2_t;

// 2D Affine Transform Matrix
// [ m00  m01  m02 ]
// [ m10  m11  m12 ]
// [  0    0    1  ]
typedef struct {
    float m[6]; // m00, m01, m02, m10, m11, m12
} transform_t;

// RGBA Color (float, 0.0-1.0)
typedef struct {
    float r;
    float g;
    float b;
    float a;
} color_f_t;

// Convert 32-bit ARGB/ABGR to color_f_t
static inline color_f_t color_to_f(uint32_t c) {
    return (color_f_t){
        .r = ((c >> 16) & 0xFF) / 255.0f,
        .g = ((c >> 8) & 0xFF) / 255.0f,
        .b = (c & 0xFF) / 255.0f,
        .a = ((c >> 24) & 0xFF) / 255.0f
    };
}

static inline uint32_t color_f_to_u32(color_f_t c) {
    uint8_t r = (uint8_t)(int)(c.r * 255.0f);
    uint8_t g = (uint8_t)(int)(c.g * 255.0f);
    uint8_t b = (uint8_t)(int)(c.b * 255.0f);
    uint8_t a = (uint8_t)(int)(c.a * 255.0f);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Basic Matrix operations
void transform_identity(transform_t *t);
void transform_translate(transform_t *t, float tx, float ty);
void transform_scale(transform_t *t, float sx, float sy);
void transform_rotate(transform_t *t, float angle_rad);
void transform_multiply(transform_t *out, const transform_t *a, const transform_t *b);
vec2_t transform_point(const transform_t *t, vec2_t p);
