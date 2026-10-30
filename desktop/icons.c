/*
 * Zarqaa OS 0.1 - Desktop Icons
 * Creator: Abdelhak Chabbi
 */

#include "desktop/icons.h"
#include "drivers/framebuffer.h"
#include "gui/graphics.h"

void icon_draw(int x, int y, int icon_id) {
    switch (icon_id) {
        case ICON_FOLDER:
            /* High-quality Vector Folder */
            // Back folder tab
            gfx_draw_rounded_rect_aa(x + 2, y + 2, 16, 6, 2, 0xFFE0B0);
            // Main folder body (Gradient-like effect using 2 rects)
            gfx_draw_rounded_rect_aa(x, y + 6, ICON_SIZE, ICON_SIZE - 6, 3, 0xFFFFCC);
            gfx_draw_rounded_rect_aa(x, y + 10, ICON_SIZE, ICON_SIZE - 10, 3, 0xFFE4B5);
            // Soft highlight line
            gfx_draw_line_aa(x + 2, y + 7, x + ICON_SIZE - 2, y + 7, 0xFFFFFF);
            break;

        case ICON_FILE:
            /* High-quality Document */
            gfx_draw_rounded_rect_aa(x + 4, y, ICON_SIZE - 8, ICON_SIZE, 2, 0xFFF8F8F8);
            // Folded corner
            fb_fill_rect(x + ICON_SIZE - 10, y, 6, 6, 0xFFE0E0E0);
            gfx_draw_line_aa(x + ICON_SIZE - 10, y + 6, x + ICON_SIZE - 4, y + 6, 0xFFCCCCCC);
            gfx_draw_line_aa(x + ICON_SIZE - 10, y, x + ICON_SIZE - 10, y + 6, 0xFFCCCCCC);
            // Document lines
            gfx_draw_line_aa(x + 8, y + 8, x + ICON_SIZE - 12, y + 8, 0xFFCCCCCC);
            gfx_draw_line_aa(x + 8, y + 14, x + ICON_SIZE - 10, y + 14, 0xFFCCCCCC);
            gfx_draw_line_aa(x + 8, y + 20, x + ICON_SIZE - 10, y + 20, 0xFFCCCCCC);
            break;

        case ICON_TERMINAL:
            /* High-quality Terminal */
            gfx_draw_rounded_rect_aa(x + 2, y + 2, ICON_SIZE - 4, ICON_SIZE - 4, 3, 0xFF1E1E1E);
            // Title bar
            gfx_draw_rounded_rect_aa(x + 2, y + 2, ICON_SIZE - 4, 6, 2, 0xFF333333);
            fb_fill_rect(x + 2, y + 6, ICON_SIZE - 4, 2, 0xFF333333); // Fill bottom corners of title
            fb_draw_string_transparent(x + 5, y + 10, ">_", COLOR_SUCCESS);
            break;

        case ICON_TEXT_EDITOR:
            /* High-quality Text Editor */
            gfx_draw_rounded_rect_aa(x + 4, y + 2, ICON_SIZE - 8, ICON_SIZE - 4, 2, 0xFFF0F0F0);
            // Header
            gfx_draw_rounded_rect_aa(x + 4, y + 2, ICON_SIZE - 8, 8, 2, COLOR_ACCENT);
            fb_fill_rect(x + 4, y + 8, ICON_SIZE - 8, 2, COLOR_ACCENT);
            // Lines
            gfx_draw_line_aa(x + 8, y + 14, x + 20, y + 14, 0xFF999999);
            gfx_draw_line_aa(x + 8, y + 19, x + 24, y + 19, 0xFF999999);
            gfx_draw_line_aa(x + 8, y + 24, x + 22, y + 24, 0xFF999999);
            break;

        case ICON_SETTINGS:
            /* Settings Gear (Anti-aliased) */
            gfx_draw_circle_aa(x + ICON_SIZE/2, y + ICON_SIZE/2, 10, 0xFF777777);
            gfx_draw_circle_aa(x + ICON_SIZE/2, y + ICON_SIZE/2, 5, COLOR_BG_DARK);
            // Notches
            fb_fill_rect(x + ICON_SIZE/2 - 2, y + 2, 4, ICON_SIZE - 4, 0xFF888888);
            fb_fill_rect(x + 2, y + ICON_SIZE/2 - 2, ICON_SIZE - 4, 4, 0xFF888888);
            break;

        case ICON_CALCULATOR:
            /* High-quality Calculator */
            gfx_draw_rounded_rect_aa(x + 4, y + 2, ICON_SIZE - 8, ICON_SIZE - 4, 3, 0xFF2D2D2D);
            gfx_draw_rounded_rect_aa(x + 6, y + 5, ICON_SIZE - 12, 7, 1, 0xFFB0C4DE);
            // Buttons
            gfx_draw_rounded_rect_aa(x + 6, y + 14, 5, 4, 1, 0xFF555555);
            gfx_draw_rounded_rect_aa(x + 13, y + 14, 5, 4, 1, 0xFF555555);
            gfx_draw_rounded_rect_aa(x + 20, y + 14, 5, 4, 1, 0xFFFFA500);
            
            gfx_draw_rounded_rect_aa(x + 6, y + 20, 5, 4, 1, 0xFF555555);
            gfx_draw_rounded_rect_aa(x + 13, y + 20, 5, 4, 1, 0xFF555555);
            gfx_draw_rounded_rect_aa(x + 20, y + 20, 5, 4, 1, 0xFF00E5FF);
            break;

        case ICON_COMPUTER:
            /* High-quality Monitor */
            gfx_draw_rounded_rect_aa(x + 2, y + 4, ICON_SIZE - 4, ICON_SIZE - 12, 2, 0xFFE0E0E0);
            gfx_draw_rounded_rect_aa(x + 4, y + 6, ICON_SIZE - 8, ICON_SIZE - 16, 1, 0xFF0A101D);
            // Screen content
            fb_fill_rect(x + 5, y + 7, ICON_SIZE - 10, ICON_SIZE - 18, 0xFF141F30);
            // Stand
            fb_fill_rect(x + 12, y + ICON_SIZE - 8, 8, 4, 0xFFB0B0B0);
            gfx_draw_rounded_rect_aa(x + 8, y + ICON_SIZE - 4, 16, 2, 1, 0xFFB0B0B0);
            break;

        case ICON_TRASH:
            /* High-quality Trash */
            gfx_draw_rounded_rect_aa(x + 6, y + 8, ICON_SIZE - 12, ICON_SIZE - 10, 2, 0xFFC0C0C0);
            // Lid
            gfx_draw_rounded_rect_aa(x + 4, y + 5, ICON_SIZE - 8, 3, 1, 0xFF909090);
            gfx_draw_rounded_rect_aa(x + 12, y + 3, 8, 3, 1, 0xFF909090);
            // Lines
            gfx_draw_line_aa(x + 10, y + 11, x + 10, y + ICON_SIZE - 4, 0xFF808080);
            gfx_draw_line_aa(x + 16, y + 11, x + 16, y + ICON_SIZE - 4, 0xFF808080);
            gfx_draw_line_aa(x + 22, y + 11, x + 22, y + ICON_SIZE - 4, 0xFF808080);
            break;

        case ICON_PAINT:
            /* Paint Palette & Brush */
            gfx_draw_circle_aa(x + 12, y + 16, 10, 0xFFE0C090); // Palette board
            gfx_draw_circle_filled(x + 7, y + 14, 2, 0xFFFF3333); // Red
            gfx_draw_circle_filled(x + 12, y + 10, 2, 0xFF33FF33); // Green
            gfx_draw_circle_filled(x + 16, y + 15, 2, 0xFF3333FF); // Blue
            // Brush handle
            gfx_draw_line_aa(x + 22, y + 2, x + 14, y + 14, 0xFF8B4513);
            gfx_draw_line_aa(x + 23, y + 3, x + 15, y + 15, 0xFF8B4513);
            // Brush tip
            fb_fill_rect(x + 13, y + 13, 3, 3, 0xFFFFFFFF);
            break;

        case ICON_APP:
        default:
            /* Modern Generic App */
            gfx_draw_rounded_rect_aa(x + 4, y + 4, ICON_SIZE - 8, ICON_SIZE - 8, 4, 0xFF8A2BE2);
            gfx_draw_circle_aa(x + ICON_SIZE/2, y + ICON_SIZE/2, 6, 0xFFFFFFFF);
            break;
    }
}

void icon_draw_futuristic(int cx, int cy, int radius, int icon_id, bool active) {
    // Base outer ring
    gfx_draw_circle_aa(cx, cy, radius, active ? COLOR_ACCENT : 0x334455);
    if (active) {
        // Inner glowing effect
        gfx_draw_circle_aa(cx, cy, radius - 1, COLOR_ACCENT);
        gfx_draw_circle_aa(cx, cy, radius - 2, 0x005577);
    }

    color_t c = active ? 0xFFFFFF : 0x88AADD;

    switch (icon_id) {
        case ICON_HUD_SYSTEM:
            gfx_draw_circle_filled(cx, cy, radius / 3, c);
            gfx_draw_circle_aa(cx, cy, radius / 2 + 1, c);
            break;
        case ICON_HUD_UPDATE:
            gfx_draw_circle_aa(cx, cy, radius / 2, c);
            gfx_draw_line_aa(cx, cy - radius/2, cx - 3, cy - radius/2 - 3, c);
            gfx_draw_line_aa(cx, cy - radius/2, cx - 3, cy - radius/2 + 3, c);
            break;
        case ICON_HUD_SECURITY:
            gfx_draw_line_aa(cx - 5, cy - 4, cx + 5, cy - 4, c);
            gfx_draw_line_aa(cx - 5, cy - 4, cx, cy + 6, c);
            gfx_draw_line_aa(cx + 5, cy - 4, cx, cy + 6, c);
            gfx_draw_circle_filled(cx, cy, 2, c);
            break;
        case ICON_HUD_MONITOR:
            gfx_draw_line_aa(cx - 6, cy, cx - 2, cy, c);
            gfx_draw_line_aa(cx - 2, cy, cx, cy - 6, c);
            gfx_draw_line_aa(cx, cy - 6, cx + 2, cy + 6, c);
            gfx_draw_line_aa(cx + 2, cy + 6, cx + 4, cy, c);
            gfx_draw_line_aa(cx + 4, cy, cx + 6, cy, c);
            break;
        case ICON_HUD_POWER:
            fb_draw_rect(cx - 4, cy - 6, 8, 12, c);
            fb_fill_rect(cx - 2, cy - 8, 4, 2, c);
            if (active) fb_fill_rect(cx - 2, cy, 4, 4, c);
            break;
        case ICON_HUD_APPS:
            fb_fill_rect(cx - 5, cy - 5, 4, 4, c);
            fb_fill_rect(cx + 1, cy - 5, 4, 4, c);
            fb_fill_rect(cx - 5, cy + 1, 4, 4, c);
            fb_fill_rect(cx + 1, cy + 1, 4, 4, c);
            break;
        case ICON_TERMINAL:
            gfx_draw_line_aa(cx - 4, cy - 3, cx, cy, c);
            gfx_draw_line_aa(cx, cy, cx - 4, cy + 3, c);
            gfx_draw_line_aa(cx + 1, cy + 3, cx + 5, cy + 3, c);
            break;
        case ICON_FOLDER:
            gfx_draw_line_aa(cx - 6, cy - 4, cx - 2, cy - 4, c);
            gfx_draw_line_aa(cx - 2, cy - 4, cx, cy - 2, c);
            gfx_draw_line_aa(cx, cy - 2, cx + 6, cy - 2, c);
            gfx_draw_line_aa(cx + 6, cy - 2, cx + 6, cy + 4, c);
            gfx_draw_line_aa(cx + 6, cy + 4, cx - 6, cy + 4, c);
            gfx_draw_line_aa(cx - 6, cy + 4, cx - 6, cy - 4, c);
            break;
        case ICON_SETTINGS:
            gfx_draw_circle_aa(cx, cy, radius/2, c);
            gfx_draw_line_aa(cx - radius/2 - 2, cy, cx + radius/2 + 2, cy, c);
            gfx_draw_line_aa(cx, cy - radius/2 - 2, cx, cy + radius/2 + 2, c);
            break;
        default:
            gfx_draw_circle_filled(cx, cy, 3, c);
            break;
    }
}

void icon_draw_bitmap(int x, int y, const uint32_t *bitmap, int w, int h) {
    if (!bitmap) return;
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            uint32_t fg = bitmap[r * w + c];
            
            // Chroma key hack: Treat pure white as fully transparent
            if (fg == 0xFFFFFFFF) continue;

            uint8_t alpha = (fg >> 24) & 0xFF;
            
            if (alpha == 255) {
                fb_putpixel(x + c, y + r, fg);
            } else if (alpha > 0) {
                // True alpha blending for anti-aliasing
                uint32_t bg = fb_getpixel(x + c, y + r);
                
                uint8_t fg_r = (fg >> 16) & 0xFF;
                uint8_t fg_g = (fg >> 8) & 0xFF;
                uint8_t fg_b = fg & 0xFF;
                
                uint8_t bg_r = (bg >> 16) & 0xFF;
                uint8_t bg_g = (bg >> 8) & 0xFF;
                uint8_t bg_b = bg & 0xFF;
                
                uint8_t out_r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
                uint8_t out_g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
                uint8_t out_b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;
                
                fb_putpixel(x + c, y + r, (0xFF << 24) | (out_r << 16) | (out_g << 8) | out_b);
            }
        }
    }
}

void icon_draw_shape(int cx, int cy, int r, int icon_id, color_t color) {
    switch (icon_id) {
        case ICON_HUD_SYSTEM:
            gfx_draw_circle_filled(cx, cy, r / 3, color);
            gfx_draw_circle(cx, cy, (r * 2) / 3, color);
            gfx_draw_circle(cx, cy, r - 2, color);
            break;
            
        case ICON_HUD_UPDATE:
            gfx_draw_circle(cx, cy, (r * 2) / 3, color);
            // Draw arrow head at top
            gfx_draw_line_aa(cx, cy - (r * 2)/3, cx - r/4, cy - (r * 2)/3 - r/4, color);
            gfx_draw_line_aa(cx, cy - (r * 2)/3, cx - r/4, cy - (r * 2)/3 + r/4, color);
            break;
            
        case ICON_HUD_SECURITY:
            // Shield outline
            gfx_draw_line_aa(cx - r/2, cy - r/2, cx + r/2, cy - r/2, color);
            gfx_draw_line_aa(cx - r/2, cy - r/2, cx - r/2, cy, color);
            gfx_draw_line_aa(cx + r/2, cy - r/2, cx + r/2, cy, color);
            gfx_draw_line_aa(cx - r/2, cy, cx, cy + r*2/3, color);
            gfx_draw_line_aa(cx + r/2, cy, cx, cy + r*2/3, color);
            // Inner checkmark
            gfx_draw_line_aa(cx - r/4, cy, cx - r/10, cy + r/5, color);
            gfx_draw_line_aa(cx - r/10, cy + r/5, cx + r/4, cy - r/6, color);
            break;
            
        case ICON_HUD_MONITOR:
            // Heartbeat/ECG line
            gfx_draw_line_aa(cx - r*3/4, cy, cx - r/3, cy, color);
            gfx_draw_line_aa(cx - r/3, cy, cx - r/6, cy - r/2, color);
            gfx_draw_line_aa(cx - r/6, cy - r/2, cx + r/6, cy + r/2, color);
            gfx_draw_line_aa(cx + r/6, cy + r/2, cx + r/3, cy, color);
            gfx_draw_line_aa(cx + r/3, cy, cx + r*3/4, cy, color);
            break;
            
        case ICON_HUD_POWER:
            // Battery representation
            fb_draw_rect(cx - r/3, cy - r/2, (r*2)/3, r, color);
            fb_fill_rect(cx - r/6, cy - r/2 - r/6, r/3, r/6, color); // tip
            fb_fill_rect(cx - r/6, cy - r/4, r/3, r/2, color); // charge level
            break;
            
        case ICON_HUD_APPS:
            // 4 boxes grid
            fb_fill_rect(cx - r/2, cy - r/2, r/3, r/3, color);
            fb_fill_rect(cx + r/6, cy - r/2, r/3, r/3, color);
            fb_fill_rect(cx - r/2, cy + r/6, r/3, r/3, color);
            fb_fill_rect(cx + r/6, cy + r/6, r/3, r/3, color);
            break;
            
        case ICON_TERMINAL:
            // CLI prompt ">_"
            gfx_draw_line_aa(cx - r/2, cy - r/3, cx - r/10, cy, color);
            gfx_draw_line_aa(cx - r/10, cy, cx - r/2, cy + r/3, color);
            gfx_draw_line_aa(cx, cy + r/3, cx + r/2, cy + r/3, color);
            break;
            
        case ICON_CALCULATOR:
            // Calculator grid
            fb_draw_rect(cx - r/2, cy - r*2/3, r, r*4/3, color);
            fb_draw_rect(cx - r/2 + 2, cy - r*2/3 + 2, r - 4, r/3, color); // screen
            fb_fill_rect(cx - r/2 + 2, cy - r/6, r/4 - 1, r/4 - 1, color);
            fb_fill_rect(cx - r/4 + 1, cy - r/6, r/4 - 1, r/4 - 1, color);
            fb_fill_rect(cx + 2, cy - r/6, r/4 - 1, r/4 - 1, color);
            fb_fill_rect(cx - r/2 + 2, cy + r/6, r/4 - 1, r/4 - 1, color);
            fb_fill_rect(cx - r/4 + 1, cy + r/6, r/4 - 1, r/4 - 1, color);
            fb_fill_rect(cx + 2, cy + r/6, r/4 - 1, r/4 - 1, color);
            break;
            
        case ICON_FOLDER:
            // Folder silhouette
            gfx_draw_line_aa(cx - r*3/4, cy - r/2, cx - r/4, cy - r/2, color);
            gfx_draw_line_aa(cx - r/4, cy - r/2, cx, cy - r/4, color);
            gfx_draw_line_aa(cx, cy - r/4, cx + r*3/4, cy - r/4, color);
            gfx_draw_line_aa(cx + r*3/4, cy - r/4, cx + r*3/4, cy + r/2, color);
            gfx_draw_line_aa(cx + r*3/4, cy + r/2, cx - r*3/4, cy + r/2, color);
            gfx_draw_line_aa(cx - r*3/4, cy + r/2, cx - r*3/4, cy - r/2, color);
            break;
            
        case ICON_SETTINGS:
            // Gear representation
            gfx_draw_circle(cx, cy, r/3, color);
            gfx_draw_circle(cx, cy, (r*2)/3, color);
            {
                static const int dxs[] = { 1,  1,  0, -1, -1, -1,  0,  1 };
                static const int dys[] = { 0,  1,  1,  1,  0, -1, -1, -1 };
                for (int step = 0; step < 8; step++) {
                    int dx = dxs[step];
                    int dy = dys[step];
                    int x_start = cx + (dx * r * 2)/3;
                    int y_start = cy + (dy * r * 2)/3;
                    int x_end = cx + (dx * r);
                    int y_end = cy + (dy * r);
                    gfx_draw_line_aa(x_start, y_start, x_end, y_end, color);
                }
            }
            break;
            
        case ICON_HUD_CUBE:
            // 3D wireframe cube
            fb_draw_rect(cx - r/3, cy - r/10, r/2, r/2, color);
            fb_draw_rect(cx - r/10, cy - r/3, r/2, r/2, color);
            gfx_draw_line_aa(cx - r/3, cy - r/10, cx - r/10, cy - r/3, color);
            gfx_draw_line_aa(cx - r/3 + r/2, cy - r/10, cx - r/10 + r/2, cy - r/3, color);
            gfx_draw_line_aa(cx - r/3, cy - r/10 + r/2, cx - r/10, cy - r/3 + r/2, color);
            gfx_draw_line_aa(cx - r/3 + r/2, cy - r/10 + r/2, cx - r/10 + r/2, cy - r/3 + r/2, color);
            break;
            
        case ICON_HUD_CHIP:
            // Microchip CPU
            fb_draw_rect(cx - r/2, cy - r/2, r, r, color);
            fb_draw_rect(cx - r/4, cy - r/4, r/2, r/2, color);
            // Pins
            gfx_draw_line_aa(cx - r*3/4, cy - r/4, cx - r/2, cy - r/4, color);
            gfx_draw_line_aa(cx - r*3/4, cy + r/4, cx - r/2, cy + r/4, color);
            gfx_draw_line_aa(cx + r/2, cy - r/4, cx + r*3/4, cy - r/4, color);
            gfx_draw_line_aa(cx + r/2, cy + r/4, cx + r*3/4, cy + r/4, color);
            gfx_draw_line_aa(cx - r/4, cy - r*3/4, cx - r/4, cy - r/2, color);
            gfx_draw_line_aa(cx + r/4, cy - r*3/4, cx + r/4, cy - r/2, color);
            gfx_draw_line_aa(cx - r/4, cy + r/2, cx - r/4, cy + r*3/4, color);
            gfx_draw_line_aa(cx + r/4, cy + r/2, cx + r/4, cy + r*3/4, color);
            break;
            
        case ICON_HUD_DATABASE:
            // Database stack cylinders
            gfx_draw_circle(cx, cy - r/3, r/2, color);
            gfx_draw_circle(cx, cy + r/3, r/2, color);
            gfx_draw_line_aa(cx - r/2, cy - r/3, cx - r/2, cy + r/3, color);
            gfx_draw_line_aa(cx + r/2, cy - r/3, cx + r/2, cy + r/3, color);
            gfx_draw_line_aa(cx - r/2, cy, cx + r/2, cy, color);
            break;
            
        case ICON_HUD_WIFI:
            // WiFi waves (arcs approximated as concentric circles)
            gfx_draw_circle_filled(cx, cy + r/2, 2, color);
            gfx_draw_circle(cx, cy + r/2, r/3, color);
            gfx_draw_circle(cx, cy + r/2, (r*2)/3, color);
            gfx_draw_circle(cx, cy + r/2, r, color);
            break;
            
        case ICON_HUD_USER:
            // User avatar (Head and shoulders)
            gfx_draw_circle_aa(cx, cy - r/4, r/3, color);
            gfx_draw_line_aa(cx - r/2, cy + r/2, cx + r/2, cy + r/2, color);
            gfx_draw_line_aa(cx - r/2, cy + r/2, cx - r/3, cy + r/10, color);
            gfx_draw_line_aa(cx + r/2, cy + r/2, cx + r/3, cy + r/10, color);
            break;
            
        case ICON_HUD_OPTIMIZATION:
            // Speedometer dial
            gfx_draw_circle_aa(cx, cy + r/4, r, color);
            gfx_draw_line_aa(cx, cy + r/4, cx + r/2, cy - r/4, color); // pointer
            gfx_draw_circle_filled(cx, cy + r/4, 3, color);
            break;
            
        case ICON_HUD_CLEANING:
            // Broom/Sparkles
            gfx_draw_line_aa(cx - r/2, cy - r/2, cx + r/4, cy + r/4, color); // Handle
            gfx_draw_line_aa(cx + r/4, cy + r/4, cx + r/2, cy + r/6, color);
            gfx_draw_line_aa(cx + r/4, cy + r/4, cx + r/6, cy + r/2, color);
            gfx_draw_line_aa(cx + r/2, cy + r/6, cx + r/6, cy + r/2, color);
            break;
            
        case ICON_HUD_PERSONALIZE:
            // Palette / Theme
            gfx_draw_circle_aa(cx, cy, r*3/4, color);
            gfx_draw_circle_filled(cx - r/4, cy - r/4, 2, color);
            gfx_draw_circle_filled(cx + r/4, cy - r/4, 2, color);
            gfx_draw_circle_filled(cx - r/4, cy + r/4, 2, color);
            break;
            
        case ICON_HUD_PRIORITY:
            // Three levels indicator
            fb_fill_rect(cx - r/2, cy + r/6, r/4, r/3, color);
            fb_fill_rect(cx - r/10, cy - r/6, r/4, r*2/3, color);
            fb_fill_rect(cx + r/3, cy - r/2, r/4, r, color);
            break;
            
        case ICON_HUD_SEARCH:
            // Magnifying glass
            gfx_draw_circle_aa(cx - r/5, cy - r/5, r/2, color);
            gfx_draw_line_aa(cx + r/10, cy + r/10, cx + r*2/3, cy + r*2/3, color);
            break;
            
        case ICON_HUD_BELL:
            // Notification bell
            gfx_draw_line_aa(cx - r/2, cy + r/3, cx + r/2, cy + r/3, color);
            gfx_draw_line_aa(cx - r/2, cy + r/3, cx - r/3, cy - r/3, color);
            gfx_draw_line_aa(cx + r/2, cy + r/3, cx + r/3, cy - r/3, color);
            gfx_draw_circle_aa(cx, cy - r/3, r/3, color);
            gfx_draw_circle_filled(cx, cy + r/2, 2, color);
            break;
            
        default:
            gfx_draw_circle_filled(cx, cy, 3, color);
            break;
    }
}
