#include "kernel/clipboard.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

static char *global_clipboard_text = NULL;

void clipboard_init(void) {
    global_clipboard_text = NULL;
}

void clipboard_set_text(const char *text) {
    if (global_clipboard_text) {
        kfree(global_clipboard_text);
        global_clipboard_text = NULL;
    }
    if (text) {
        size_t len = strlen(text);
        global_clipboard_text = kmalloc(len + 1);
        if (global_clipboard_text) {
            strcpy(global_clipboard_text, text);
        }
    }
}

const char *clipboard_get_text(void) {
    return global_clipboard_text;
}
