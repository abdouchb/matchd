#include "graphics/libvector/bezier.h"

static vec2_t bezier_quad_eval(vec2_t p0, vec2_t p1, vec2_t p2, float t) {
    float u = 1.0f - t;
    float w0 = u * u;
    float w1 = 2.0f * u * t;
    float w2 = t * t;
    
    return (vec2_t){
        .x = w0 * p0.x + w1 * p1.x + w2 * p2.x,
        .y = w0 * p0.y + w1 * p1.y + w2 * p2.y
    };
}

static vec2_t bezier_cubic_eval(vec2_t p0, vec2_t p1, vec2_t p2, vec2_t p3, float t) {
    float u = 1.0f - t;
    float u2 = u * u;
    float u3 = u2 * u;
    float t2 = t * t;
    float t3 = t2 * t;
    
    float w0 = u3;
    float w1 = 3.0f * u2 * t;
    float w2 = 3.0f * u * t2;
    float w3 = t3;
    
    return (vec2_t){
        .x = w0 * p0.x + w1 * p1.x + w2 * p2.x + w3 * p3.x,
        .y = w0 * p0.y + w1 * p1.y + w2 * p2.y + w3 * p3.y
    };
}

int bezier_quad_flatten(vec2_t p0, vec2_t p1, vec2_t p2, float tolerance, vec2_t *out_points, int max_points) {
    if (max_points < 2) return 0;
    
    // Simple adaptive or fixed step flattening. For simplicity, fixed steps.
    int steps = 10; // We can make this adaptive based on length and tolerance
    int count = 0;
    
    for (int i = 0; i <= steps && count < max_points; i++) {
        float t = (float)i / (float)steps;
        out_points[count++] = bezier_quad_eval(p0, p1, p2, t);
    }
    
    return count;
}

int bezier_cubic_flatten(vec2_t p0, vec2_t p1, vec2_t p2, vec2_t p3, float tolerance, vec2_t *out_points, int max_points) {
    if (max_points < 2) return 0;
    
    int steps = 20; // Fixed step for now
    int count = 0;
    
    for (int i = 0; i <= steps && count < max_points; i++) {
        float t = (float)i / (float)steps;
        out_points[count++] = bezier_cubic_eval(p0, p1, p2, p3, t);
    }
    
    return count;
}
