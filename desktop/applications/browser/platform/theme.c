#include "theme.h"

static browser_theme_t current_theme;

void browser_theme_init(void) {
    // Nexus Dark Glass Theme
    current_theme.bg_base = 0xFF0F172A;       // Deep Slate
    current_theme.bg_surface = 0xFF1E1B4B;    // Deep Purple Indigo
    current_theme.bg_overlay = 0xFF2D2A5D;    // Lighter overlay
    current_theme.text_primary = 0xFFFFFFFF;  // White
    current_theme.text_secondary = 0xFFA0AEC0;// Gray/Silver
    current_theme.accent_primary = 0xFF8B5CF6;// Vibrant Purple
    current_theme.border_color = 0xFF333344;  // Subtle border
}

const browser_theme_t *browser_theme_get_current(void) {
    return &current_theme;
}
