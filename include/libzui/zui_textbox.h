#ifndef LIBZUI_ZUI_TEXTBOX_H
#define LIBZUI_ZUI_TEXTBOX_H

#include "libzui/zui.h"

typedef struct zui_textbox zui_textbox_t;

struct zui_textbox {
    zui_widget_t base;
    char *text;
    int max_length;
    
    int cursor_pos;
    int selection_start;
    int selection_end;
    
    bool is_focused;
    bool is_selecting; // Mouse drag state
};

zui_textbox_t *zui_textbox_create(int x, int y, int width, int height, const char *initial_text, int max_length);
void zui_textbox_set_text(zui_textbox_t *tb, const char *text);
const char *zui_textbox_get_text(zui_textbox_t *tb);

// Helper to handle keyboard events for focused textboxes
bool zui_textbox_handle_key(zui_textbox_t *tb, char c, uint16_t scancode);

#endif
