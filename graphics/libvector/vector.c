#include "graphics/libvector/vector.h"

#define ZAR_PI 3.14159265358979323846f

static float zar_sinf(float x) {
    while (x > ZAR_PI) x -= 2.0f * ZAR_PI;
    while (x < -ZAR_PI) x += 2.0f * ZAR_PI;
    if (x < 0) return -zar_sinf(-x);
    float num = 16.0f * x * (ZAR_PI - x);
    float den = 5.0f * ZAR_PI * ZAR_PI - 4.0f * x * (ZAR_PI - x);
    return num / den;
}

static float zar_cosf(float x) {
    return zar_sinf(x + ZAR_PI / 2.0f);
}

void transform_identity(transform_t *t) {
    t->m[0] = 1.0f; t->m[1] = 0.0f; t->m[2] = 0.0f;
    t->m[3] = 0.0f; t->m[4] = 1.0f; t->m[5] = 0.0f;
}

void transform_translate(transform_t *t, float tx, float ty) {
    transform_t trans;
    transform_identity(&trans);
    trans.m[2] = tx;
    trans.m[5] = ty;
    
    transform_t result;
    transform_multiply(&result, t, &trans);
    *t = result;
}

void transform_scale(transform_t *t, float sx, float sy) {
    transform_t scale;
    transform_identity(&scale);
    scale.m[0] = sx;
    scale.m[4] = sy;
    
    transform_t result;
    transform_multiply(&result, t, &scale);
    *t = result;
}

void transform_rotate(transform_t *t, float angle_rad) {
    float c = zar_cosf(angle_rad);
    float s = zar_sinf(angle_rad);
    
    transform_t rot;
    rot.m[0] = c;  rot.m[1] = -s; rot.m[2] = 0.0f;
    rot.m[3] = s;  rot.m[4] = c;  rot.m[5] = 0.0f;
    
    transform_t result;
    transform_multiply(&result, t, &rot);
    *t = result;
}

void transform_multiply(transform_t *out, const transform_t *a, const transform_t *b) {
    out->m[0] = a->m[0] * b->m[0] + a->m[1] * b->m[3];
    out->m[1] = a->m[0] * b->m[1] + a->m[1] * b->m[4];
    out->m[2] = a->m[0] * b->m[2] + a->m[1] * b->m[5] + a->m[2];
    
    out->m[3] = a->m[3] * b->m[0] + a->m[4] * b->m[3];
    out->m[4] = a->m[3] * b->m[1] + a->m[4] * b->m[4];
    out->m[5] = a->m[3] * b->m[2] + a->m[4] * b->m[5] + a->m[5];
}

vec2_t transform_point(const transform_t *t, vec2_t p) {
    vec2_t r;
    r.x = t->m[0] * p.x + t->m[1] * p.y + t->m[2];
    r.y = t->m[3] * p.x + t->m[4] * p.y + t->m[5];
    return r;
}
