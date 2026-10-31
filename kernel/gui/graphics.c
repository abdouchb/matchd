/*
 * Zarqaa OS 0.1 - Graphics Primitives
 * Creator: Abdelhak Chabbi
 */

#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "zarqaa/types.h"

#define MAX_CORNER_RADIUS 32
static uint8_t g_corner_mask[MAX_CORNER_RADIUS + 1][MAX_CORNER_RADIUS * MAX_CORNER_RADIUS];
static bool g_corner_cache_ready = false;

void gfx_init_corner_cache(void) {
    if (g_corner_cache_ready) return;

    for (int r = 1; r <= MAX_CORNER_RADIUS; r++) {
        int r_sq = r * r;
        for (int cy = 0; cy < r; cy++) {
            for (int cx = 0; cx < r; cx++) {
                int hits = 0;
                for (int sy = 0; sy < 4; sy++) {
                    int dy = (r * 4 - 2) - (cy * 4 + sy);
                    for (int sx = 0; sx < 4; sx++) {
                        int dx = (r * 4 - 2) - (cx * 4 + sx);
                        if (dx * dx + dy * dy <= r_sq * 16) {
                            hits++;
                        }
                    }
                }
                g_corner_mask[r][cy * r + cx] = (uint8_t)((hits * 255) / 16);
            }
        }
    }
    g_corner_cache_ready = true;
}

void gfx_draw_rounded_panel_aa_clipped(int x, int y, int w, int h, int radius,
                                       color_t color, uint8_t base_alpha, const rect_t *clip) {
    if (w <= 0 || h <= 0 || base_alpha == 0) return;
    if (!g_corner_cache_ready) gfx_init_corner_cache();

    if (radius > MAX_CORNER_RADIUS) radius = MAX_CORNER_RADIUS;
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;

    /* 1. Fast Path Clip-Aware Rectangle Fill for Center Spans */
    if (base_alpha == 255) {
        fb_fill_rect_clipped(x + radius, y, w - 2 * radius, h, color, clip);
        fb_fill_rect_clipped(x, y + radius, radius, h - 2 * radius, color, clip);
        fb_fill_rect_clipped(x + w - radius, y + radius, radius, h - 2 * radius, color, clip);
    } else {
        fb_fill_rect_alpha_clipped(x + radius, y, w - 2 * radius, h, color, base_alpha, clip);
        fb_fill_rect_alpha_clipped(x, y + radius, radius, h - 2 * radius, color, base_alpha, clip);
        fb_fill_rect_alpha_clipped(x + w - radius, y + radius, radius, h - 2 * radius, color, base_alpha, clip);
    }

    /* 2. Zero-cost 4 Corners rendering via precomputed LUT (0 sqrt, 0 float) */
    const uint8_t *mask = &g_corner_mask[radius][0];

    for (int cy = 0; cy < radius; cy++) {
        int row_idx = cy * radius;
        for (int cx = 0; cx < radius; cx++) {
            uint8_t m_alpha = mask[row_idx + cx];
            if (m_alpha == 0) continue;

            uint8_t final_a = (base_alpha == 255) ? m_alpha : (uint8_t)((m_alpha * base_alpha) / 255);

            fb_blend_pixel_clipped(x + cx, y + cy, color, final_a, clip);                     /* Top-Left */
            fb_blend_pixel_clipped(x + w - 1 - cx, y + cy, color, final_a, clip);             /* Top-Right */
            fb_blend_pixel_clipped(x + cx, y + h - 1 - cy, color, final_a, clip);             /* Bottom-Left */
            fb_blend_pixel_clipped(x + w - 1 - cx, y + h - 1 - cy, color, final_a, clip);     /* Bottom-Right */
        }
    }
}

void gfx_draw_rounded_panel_aa(int x, int y, int w, int h, int radius, color_t color, uint8_t alpha) {
    gfx_draw_rounded_panel_aa_clipped(x, y, w, h, radius, color, alpha, NULL);
}

// Forward declarations
static uint32_t isqrt(uint32_t n);

static inline color_t fast_alpha_blend(color_t fg, color_t bg, uint8_t alpha) {
    if (alpha == 255) return fg | 0xFF000000;
    if (alpha == 0) return bg | 0xFF000000;
    uint32_t inv_alpha = 255 - alpha;
    uint32_t rb = (((fg & 0x00FF00FF) * alpha + (bg & 0x00FF00FF) * inv_alpha + 0x00800080) >> 8) & 0x00FF00FF;
    uint32_t g  = (((fg & 0x0000FF00) * alpha + (bg & 0x0000FF00) * inv_alpha + 0x00008000) >> 8) & 0x0000FF00;
    return 0xFF000000 | rb | g;
}

static inline void plot_aa(int x, int y, color_t color, uint8_t alpha) {
    if (alpha == 0) return;
    if (alpha == 255) {
        fb_putpixel(x, y, color);
        return;
    }
    color_t bg = fb_getpixel(x, y);
    fb_putpixel(x, y, fast_alpha_blend(color, bg, alpha));
}

color_t gfx_alpha_blend(color_t fg, color_t bg, uint8_t alpha) {
    return fast_alpha_blend(fg, bg, alpha);
}

void gfx_draw_gradient_v(int x, int y, int w, int h, color_t top_color, color_t bottom_color) {
    if (w <= 0 || h <= 0) return;
    uint32_t tr = (top_color >> 16) & 0xFF;
    uint32_t tg = (top_color >> 8) & 0xFF;
    uint32_t tb = top_color & 0xFF;

    uint32_t br = (bottom_color >> 16) & 0xFF;
    uint32_t bg = (bottom_color >> 8) & 0xFF;
    uint32_t bb = bottom_color & 0xFF;

    for (int i = 0; i < h; i++) {
        uint32_t r = tr + ((br - tr) * i / h);
        uint32_t g = tg + ((bg - tg) * i / h);
        uint32_t b = tb + ((bb - tb) * i / h);
        color_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;
        fb_draw_line(x, y + i, x + w - 1, y + i, color);
    }
}

void gfx_draw_gradient_h(int x, int y, int w, int h, color_t left_color, color_t right_color) {
    if (w <= 0 || h <= 0) return;
    uint32_t lr = (left_color >> 16) & 0xFF;
    uint32_t lg = (left_color >> 8) & 0xFF;
    uint32_t lb = left_color & 0xFF;

    uint32_t rr = (right_color >> 16) & 0xFF;
    uint32_t rg = (right_color >> 8) & 0xFF;
    uint32_t rb = right_color & 0xFF;

    for (int i = 0; i < w; i++) {
        uint32_t r = lr + ((rr - lr) * i / w);
        uint32_t g = lg + ((rg - lg) * i / w);
        uint32_t b = lb + ((rb - lb) * i / w);
        color_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;
        fb_draw_line(x + i, y, x + i, y + h - 1, color);
    }
}

static uint32_t isqrt(uint32_t n) {
    uint32_t root = 0;
    uint32_t bit = 1UL << 30;
    while (bit > n) bit >>= 2;
    while (bit != 0) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

void gfx_draw_circle(int cx, int cy, int r, color_t color) {
    if (r <= 0) return;
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    
    while (y >= x) {
        plot_aa(cx + x, cy + y, color, 255);
        plot_aa(cx - x, cy + y, color, 255);
        plot_aa(cx + x, cy - y, color, 255);
        plot_aa(cx - x, cy - y, color, 255);
        plot_aa(cx + y, cy + x, color, 255);
        plot_aa(cx - y, cy + x, color, 255);
        plot_aa(cx + y, cy - x, color, 255);
        plot_aa(cx - y, cy - x, color, 255);
        
        if (d <= 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void gfx_draw_circle_aa(int cx, int cy, int r, color_t color) {
    if (r <= 0) return;
    uint32_t r_scaled = r * 256;
    int r_inner_sq = (r > 1) ? (r - 1) * (r - 1) : 0;

    for (int y = -r; y <= r; y++) {
        int y2 = y * y;
        int dx_limit = (y2 < r * r) ? isqrt(r * r - y2) : 0;

        // Fill inner span with zero-overhead line
        if (dx_limit > 1) {
            fb_draw_line(cx - dx_limit + 1, cy + y, cx + dx_limit - 1, cy + y, color);
        }

        // Anti-alias edge boundary
        for (int x = dx_limit - 1; x <= dx_limit + 1; x++) {
            if (x < 0) continue;
            int d2 = x * x + y2;
            if (d2 <= r_inner_sq) continue; // Already covered by inner fill
            
            uint32_t d_scaled = isqrt(d2 * 65536);
            if (d_scaled <= r_scaled + 128) {
                uint8_t alpha = 255;
                if (d_scaled > r_scaled - 128) {
                    int delta = (int)d_scaled - (int)(r_scaled - 128);
                    int a = 255 - (delta * 255 / 256);
                    alpha = (a < 0) ? 0 : ((a > 255) ? 255 : (uint8_t)a);
                }
                plot_aa(cx + x, cy + y, color, alpha);
                if (x > 0) plot_aa(cx - x, cy + y, color, alpha);
            }
        }
    }
}

void gfx_draw_circle_filled(int cx, int cy, int r, color_t color) {
    gfx_draw_circle_aa(cx, cy, r, color);
}

void gfx_draw_rounded_rect_aa(int x, int y, int w, int h, int radius, color_t color) {
    uint8_t base_alpha = (color >> 24) & 0xFF;
    if (base_alpha == 0) return;

    if (radius <= 0) {
        if (base_alpha < 255) fb_fill_rect_alpha(x, y, w, h, color, base_alpha);
        else fb_fill_rect(x, y, w, h, color);
        return;
    }
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;

    if (base_alpha < 255) {
        fb_fill_rect_alpha(x + radius, y, w - 2 * radius, h, color, base_alpha);
        fb_fill_rect_alpha(x, y + radius, radius, h - 2 * radius, color, base_alpha);
        fb_fill_rect_alpha(x + w - radius, y + radius, radius, h - 2 * radius, color, base_alpha);
    } else {
        fb_fill_rect(x + radius, y, w - 2 * radius, h, color);
        fb_fill_rect(x, y + radius, radius, h - 2 * radius, color);
        fb_fill_rect(x + w - radius, y + radius, radius, h - 2 * radius, color);
    }

    // High-precision subpixel corner anti-aliasing with early-out math
    uint32_t r_scaled = radius * 256;
    uint32_t r_inner_bound = (r_scaled > 128) ? (r_scaled - 128) : 0;
    uint32_t r_inner_sq = r_inner_bound * r_inner_bound;
    uint32_t r_outer_bound = r_scaled + 128;
    uint32_t r_outer_sq = r_outer_bound * r_outer_bound;

    for (int cy = 0; cy < radius; cy++) {
        int dy_fixed = (radius - cy) * 256 - 128;
        int dy_sq = dy_fixed * dy_fixed;
        for (int cx = 0; cx < radius; cx++) {
            int dx_fixed = (radius - cx) * 256 - 128;
            uint32_t dist_sq = dx_fixed * dx_fixed + dy_sq;

            if (dist_sq <= r_inner_sq) {
                // Fully inside corner - skip square root!
                if (base_alpha > 0) {
                    plot_aa(x + cx, y + cy, color, base_alpha);                 // TL
                    plot_aa(x + w - 1 - cx, y + cy, color, base_alpha);         // TR
                    plot_aa(x + cx, y + h - 1 - cy, color, base_alpha);         // BL
                    plot_aa(x + w - 1 - cx, y + h - 1 - cy, color, base_alpha); // BR
                }
            } else if (dist_sq <= r_outer_sq) {
                // Boundary pixel: compute smooth AA edge
                uint32_t d_scaled = isqrt(dist_sq);
                int delta = (int)d_scaled - (int)(r_scaled - 128);
                int a = 255 - (delta * 255 / 256);
                uint8_t edge_alpha = (a < 0) ? 0 : ((a > 255) ? 255 : (uint8_t)a);
                uint8_t final_alpha = (edge_alpha * base_alpha) / 255;
                if (final_alpha > 0) {
                    plot_aa(x + cx, y + cy, color, final_alpha);                 // TL
                    plot_aa(x + w - 1 - cx, y + cy, color, final_alpha);         // TR
                    plot_aa(x + cx, y + h - 1 - cy, color, final_alpha);         // BL
                    plot_aa(x + w - 1 - cx, y + h - 1 - cy, color, final_alpha); // BR
                }
            }
        }
    }
}

void gfx_draw_rounded_rect_filled(int x, int y, int w, int h, int radius, color_t color) {
    gfx_draw_rounded_rect_aa(x, y, w, h, radius, color);
}

void gfx_draw_rounded_rect(int x, int y, int w, int h, int radius, color_t color) {
    uint8_t base_alpha = (color >> 24) & 0xFF;
    if (base_alpha == 0) return;

    if (radius <= 0) {
        if (base_alpha < 255) {
            fb_fill_rect_alpha(x, y, w, 1, color, base_alpha);
            fb_fill_rect_alpha(x, y + h - 1, w, 1, color, base_alpha);
            fb_fill_rect_alpha(x, y + 1, 1, h - 2, color, base_alpha);
            fb_fill_rect_alpha(x + w - 1, y + 1, 1, h - 2, color, base_alpha);
        } else {
            fb_draw_line(x, y, x + w - 1, y, color);
            fb_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
            fb_draw_line(x, y + 1, x, y + h - 2, color);
            fb_draw_line(x + w - 1, y + 1, x + w - 1, y + h - 2, color);
        }
        return;
    }
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;

    // Straight border lines
    if (base_alpha < 255) {
        fb_fill_rect_alpha(x + radius, y, w - 2 * radius, 1, color, base_alpha);
        fb_fill_rect_alpha(x + radius, y + h - 1, w - 2 * radius, 1, color, base_alpha);
        fb_fill_rect_alpha(x, y + radius, 1, h - 2 * radius, color, base_alpha);
        fb_fill_rect_alpha(x + w - 1, y + radius, 1, h - 2 * radius, color, base_alpha);
    } else {
        fb_draw_line(x + radius, y, x + w - radius - 1, y, color); // Top
        fb_draw_line(x + radius, y + h - 1, x + w - radius - 1, y + h - 1, color); // Bottom
        fb_draw_line(x, y + radius, x, y + h - radius - 1, color); // Left
        fb_draw_line(x + w - 1, y + radius, x + w - 1, y + h - radius - 1, color); // Right
    }

    uint32_t r_scaled = radius * 256;
    for (int cy = 0; cy < radius; cy++) {
        int dy_fixed = (radius - cy) * 256 - 128;
        int dy_sq = dy_fixed * dy_fixed;
        for (int cx = 0; cx < radius; cx++) {
            int dx_fixed = (radius - cx) * 256 - 128;
            uint32_t dist_sq = dx_fixed * dx_fixed + dy_sq;
            uint32_t d_scaled = isqrt(dist_sq);

            int diff = (int)d_scaled - (int)r_scaled;
            if (diff < 0) diff = -diff;

            if (diff <= 192) {
                int a = 255 - (diff * 255 / 192);
                uint8_t stroke_alpha = (a < 0) ? 0 : ((a > 255) ? 255 : (uint8_t)a);
                uint8_t final_alpha = (stroke_alpha * base_alpha) / 255;

                if (final_alpha > 0) {
                    plot_aa(x + cx, y + cy, color, final_alpha);                 // TL
                    plot_aa(x + w - 1 - cx, y + cy, color, final_alpha);         // TR
                    plot_aa(x + cx, y + h - 1 - cy, color, final_alpha);         // BL
                    plot_aa(x + w - 1 - cx, y + h - 1 - cy, color, final_alpha); // BR
                }
            }
        }
    }
}

/* Glass panel: Smooth anti-aliased panel with subtle top reflection shine */
void gfx_draw_glass_panel(int x, int y, int w, int h, int radius,
                          color_t fill, color_t border, uint8_t alpha) {
    // Embed the given alpha channel into the fill color
    color_t blend_fill = (fill & 0x00FFFFFF) | ((color_t)alpha << 24);
    gfx_draw_rounded_rect_aa(x, y, w, h, radius, blend_fill);
    
    // Top-edge inner glass shine for macOS/modern depth (subtle alpha blended)
    if (w > radius * 2 + 6 && h > 20) {
        for (int sx = x + radius + 1; sx <= x + w - radius - 2; sx++) {
            plot_aa(sx, y + 1, 0xFFFFFFFF, 25);
        }
    }
    
    // Similarly for border if needed, but currently border is opaque
    color_t blend_border = (border & 0x00FFFFFF) | ((color_t)alpha << 24);
    gfx_draw_rounded_rect(x, y, w, h, radius, blend_border);
}

/* Dashed line (horizontal or vertical) */
void gfx_draw_dashed_line(int x0, int y0, int x1, int y1,
                          color_t color, int dash, int gap) {
    int dx = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
    int dy = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;
    int steps;
    if (dx != 0) steps = (x1 - x0) * dx;
    else steps = (y1 - y0) * dy;
    
    int x = x0, y = y0;
    for (int i = 0; i <= steps; i++) {
        int cycle = i % (dash + gap);
        if (cycle < dash) {
            fb_putpixel(x, y, color);
        }
        x += dx;
        y += dy;
    }
}

/* Progress ring: partial circle arc for system score */
void gfx_draw_progress_ring(int cx, int cy, int r, int percent,
                            color_t bg_color, color_t fg_color) {
    /* Draw background circle */
    gfx_draw_circle(cx, cy, r, bg_color);
    
    /* Draw foreground arc up to percent */
    int target = (360 * percent) / 100;
    for (int angle = -90; angle < -90 + target; angle++) {
        /* Simple approximation: draw pixels at the ring boundary */
        /* Use fixed-point sin/cos table for kernel (no floating point!) */
        int a = angle;
        if (a < 0) a += 360;
        
        /* Very simple integer trig: step through and draw circle outline */
        for (int t = 0; t < 360; t++) {
            if ((uint32_t)t > (uint32_t)target) break;
            int shifted_t = t - 90;
            if (shifted_t < 0) shifted_t += 360;
            /* Use Bresenham-like approach: check all border pixels */
        }
        break; /* Use simple filled approach instead */
    }
    
    /* Simpler approach: draw thick arc using circle scan */
    int r_inner = r - 2;
    int r_outer = r + 1;
    for (int y = -r_outer; y <= r_outer; y++) {
        for (int x = -r_outer; x <= r_outer; x++) {
            int d2 = x*x + y*y;
            if (d2 >= r_inner*r_inner && d2 <= r_outer*r_outer) {
                /* Calculate angle (0-359, starting from top) */
                /* atan2 approximation using octant method */
                int angle;
                if (x == 0 && y == 0) continue;
                
                /* Simple angle estimation in degrees */
                int ax = x < 0 ? -x : x;
                int ay = y < 0 ? -y : y;
                
                if (ax == 0) angle = 90;
                else if (ay == 0) angle = 0;
                else if (ax >= ay) angle = (45 * ay) / ax;
                else angle = 90 - (45 * ax) / ay;
                
                /* Map to quadrants (from top=0, clockwise) */
                if (x >= 0 && y < 0) {
                    /* Q1: Top-Right (0 to 90) */
                    angle = 90 - angle;
                } else if (x >= 0 && y >= 0) {
                    /* Q2: Bottom-Right (90 to 180) */
                    angle = 90 + angle;
                } else if (x < 0 && y >= 0) {
                    /* Q3: Bottom-Left (180 to 270) */
                    angle = 270 - angle;
                } else {
                    /* Q4: Top-Left (270 to 360) */
                    angle = 270 + angle;
                }
                
                angle = angle % 360;
                
                int fill_angle = (360 * percent) / 100;
                if (angle <= fill_angle) {
                    fb_putpixel(cx + x, cy + y, fg_color);
                }
            }
        }
    }
}

static void putpixel_aa_int(int x, int y, color_t color, uint32_t a) {
    if (a == 0) return;
    if (a >= 255) {
        fb_putpixel(x, y, color);
        return;
    }
    color_t bg = fb_getpixel(x, y);
    fb_putpixel(x, y, fast_alpha_blend(color, bg, (uint8_t)a));
}

void gfx_draw_line_aa(int x0, int y0, int x1, int y1, color_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int abs_dx = dx < 0 ? -dx : dx;
    int abs_dy = dy < 0 ? -dy : dy;
    
    bool steep = abs_dy > abs_dx;
    if (steep) {
        int t;
        t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
    }
    if (x0 > x1) {
        int t;
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }
    
    dx = x1 - x0;
    dy = y1 - y0;
    
    if (dx == 0) {
        for (int x = x0; x <= x1; x++) {
            if (steep) fb_putpixel(y0, x, color);
            else fb_putpixel(x, y0, color);
        }
        return;
    }
    
    int32_t gradient = ((int32_t)dy << 16) / dx;
    int32_t intery = (y0 << 16) + (gradient / 2); 
    
    for (int x = x0; x <= x1; x++) {
        int y = intery >> 16;
        uint32_t frac = intery & 0xFFFF;
        uint32_t a = (frac * 255) >> 16;
        
        if (steep) {
            putpixel_aa_int(y,   x, color, 255 - a);
            putpixel_aa_int(y+1, x, color, a);
        } else {
            putpixel_aa_int(x, y,   color, 255 - a);
            putpixel_aa_int(x, y+1, color, a);
        }
        intery += gradient;
    }
}

