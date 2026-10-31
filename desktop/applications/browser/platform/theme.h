#ifndef BROWSER_THEME_H
#define BROWSER_THEME_H

#include "zarqaa/types.h"

typedef struct {
    color_t bg_base;
    color_t bg_surface;
    color_t bg_overlay;
    color_t text_primary;
    color_t text_secondary;
    color_t accent_primary;
    color_t border_color;
} browser_theme_t;

void browser_theme_init(void);
const browser_theme_t *browser_theme_get_current(void);

#endif
