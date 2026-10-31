/*
 * Zarqaa OS 0.1 - File Manager Application
 * Creator: Abdelhak Chabbi
 */

#include "desktop/window_manager.h"
#include "desktop/icons.h"
#include "drivers/framebuffer.h"
#include "gui/graphics.h"
#include "filesystem/vfs.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"
#include "drivers/usb/usb_hid.h"

extern struct window *text_editor_open_file(const char *path);
extern struct window *browser_open_url(const char *url);
extern struct window *python_ide_open_file(const char *path);
extern struct window *audio_player_open_file(const char *path);
extern struct window *image_viewer_open_file(const char *path);

struct fm_state {
    char current_path[256];
    char history[32][256];
    int history_idx;
    int history_max;
    
    struct vfs_dirent entries[64];
    int entry_count;
    int selected_entry;
    bool creating_file;
    bool creating_dir;
    char new_file_name[32];
    int new_file_pos;
    
    int scroll_y;
    bool dragging;
    int last_my;
    
    /* Context Menu */
    bool ctx_open;
    int  ctx_x;
    int  ctx_y;
    int  ctx_target; /* -1 = empty space, else entry index */
    
    /* Clipboard */
    char clipboard_path[256];
    bool clipboard_is_cut;
    
    /* Rename mode */
    bool renaming;
};

/* ─── helpers ─────────────────────────────────────────────── */

static void refresh_dir(struct fm_state *state) {
    struct vfs_node *dir = vfs_open(state->current_path);
    if (dir && dir->type == VFS_DIRECTORY) {
        state->entry_count = vfs_readdir(dir, state->entries, 64);
        state->selected_entry = -1;
    }
}

static void fm_goto(struct fm_state *state, const char *path) {
    if (strcmp(state->current_path, path) == 0) return;
    strcpy(state->history[state->history_idx], state->current_path);
    state->history_idx++;
    if (state->history_idx >= 32) {
        for (int i = 1; i < 32; i++)
            strcpy(state->history[i-1], state->history[i]);
        state->history_idx = 31;
    }
    state->history_max = state->history_idx;
    strcpy(state->current_path, path);
    refresh_dir(state);
}

static void get_lowercase_ext(const char *filename, char *ext_out, int max_len) {
    ext_out[0] = '\0';
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return;
    dot++;
    int i = 0;
    while (*dot && i < max_len - 1) {
        char c = *dot;
        if (c >= 'A' && c <= 'Z') c += 32;
        ext_out[i++] = c;
        dot++;
    }
    ext_out[i] = '\0';
}

static void open_file_smart(const char *path) {
    char ext[16];
    get_lowercase_ext(path, ext, sizeof(ext));
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) {
        char file_url[256];
        strcpy(file_url, "file://");
        strcat(file_url, path);
        browser_open_url(file_url);
    } else if (strcmp(ext, "py") == 0) {
        python_ide_open_file(path);
    } else if (strcmp(ext, "mp3") == 0 || strcmp(ext, "wav") == 0) {
        audio_player_open_file(path);
    } else if (strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0 ||
               strcmp(ext, "jpeg") == 0 || strcmp(ext, "bmp") == 0) {
        image_viewer_open_file(path);
    } else if (strcmp(ext, "elf") == 0 || strcmp(ext, "bin") == 0) {
        extern struct window *wm_find_window_by_title(const char *title);
        extern struct window *terminal_open(void);
        extern void wm_focus_window(struct window *win);
        struct window *term_win = wm_find_window_by_title("Terminal");
        if (!term_win) {
            terminal_open();
        } else {
            if (term_win->minimized) term_win->minimized = false;
            wm_focus_window(term_win);
        }
        extern int process_exec(const char *path, const char *name);
        process_exec(path, path);
    } else {
        text_editor_open_file(path);
    }
}

/* ─── draw ────────────────────────────────────────────────── */

static void fm_draw(struct window *win) {
    struct fm_state *state = (struct fm_state *)win->user_data;
    
    int sidebar_w = 140;
    int topbar_h  = 40;
    
    /* clamp scroll */
    int grid_w   = win->width - sidebar_w - 40;
    int cols     = grid_w / 120;
    if (cols < 1) cols = 1;
    int rows      = (state->entry_count + cols - 1) / cols;
    int max_scroll = (rows * 80) - (win->height - topbar_h - 20);
    if (max_scroll < 0) max_scroll = 0;
    if (state->scroll_y < 0) state->scroll_y = 0;
    if (state->scroll_y > max_scroll) state->scroll_y = max_scroll;

    #define GLASS_BG        0x22000000
    #define GLASS_SIDEBAR   0x44000000
    #define GLASS_TOPBAR    0x33000000
    #define GLASS_HOVER     0x44FFFFFF
    #define GLASS_SELECT    0x6636BAFF
    #define GLASS_TEXT      0xFFE8F6FF
    #define GLASS_TEXT_MUTED 0x99E8F6FF
    #define GLASS_BUTTON    0x33FFFFFF
    #define GLASS_ERROR     0x66FF4444
    #define GLASS_SUCCESS   0x6644FF44
    
    /* Background is purely translucent glass */
    fb_fill_rect(0, 0, win->width, win->height, GLASS_BG);
    
    /* Sidebar */
    fb_fill_rect(0, 0, sidebar_w, win->height, GLASS_SIDEBAR);
    fb_draw_string_transparent(10, 20, "Places", GLASS_TEXT);
    if (strcmp(state->current_path, "/") == 0)
        gfx_draw_rounded_rect_filled(5, 45, sidebar_w - 10, 30, 4, GLASS_HOVER);
    fb_draw_string_transparent(20, 52, "[ / ] Root", GLASS_TEXT);
    if (strncmp(state->current_path, "/hd0", 4) == 0 && strlen(state->current_path) <= 5)
        gfx_draw_rounded_rect_filled(5, 85, sidebar_w - 10, 30, 4, GLASS_HOVER);
    fb_draw_string_transparent(20, 92, "[/hd0] Disk", GLASS_TEXT);

    /* Top toolbar */
    fb_fill_rect(sidebar_w, 0, win->width - sidebar_w, topbar_h, GLASS_TOPBAR);
    color_t back_color = (state->history_idx > 0) ? GLASS_TEXT : GLASS_TEXT_MUTED;
    color_t fwd_color  = (state->history_idx < state->history_max) ? GLASS_TEXT : GLASS_TEXT_MUTED;
    
    gfx_draw_rounded_rect_filled(sidebar_w + 5,  5, 30, 30, 4, GLASS_BUTTON);
    fb_draw_string_transparent(sidebar_w + 15, 12, "<", back_color);
    
    gfx_draw_rounded_rect_filled(sidebar_w + 40, 5, 30, 30, 4, GLASS_BUTTON);
    fb_draw_string_transparent(sidebar_w + 50, 12, ">", fwd_color);
    
    fb_draw_string_transparent(sidebar_w + 80,  12, "Path:", GLASS_TEXT_MUTED);
    
    /* Current Path Background Pill */
    gfx_draw_rounded_rect_filled(sidebar_w + 120, 5, 200, 30, 4, GLASS_BUTTON);
    fb_draw_string_transparent(sidebar_w + 130, 12, state->current_path, GLASS_TEXT);

    int btn_x = win->width - 100;
    if (state->selected_entry != -1) {
        gfx_draw_rounded_rect_filled(btn_x, 5, 90, 30, 4, GLASS_ERROR);
        fb_draw_string_transparent(btn_x + 15, 12, "Delete", GLASS_TEXT);
        btn_x -= 100;
    }
    gfx_draw_rounded_rect_filled(btn_x, 5, 90, 30, 4, GLASS_BUTTON);
    fb_draw_string_transparent(btn_x + 10, 12, "+ Folder", GLASS_TEXT);
    btn_x -= 100;
    gfx_draw_rounded_rect_filled(btn_x, 5, 90, 30, 4, GLASS_SUCCESS);
    fb_draw_string_transparent(btn_x + 15, 12, "+ File", GLASS_TEXT);

    /* Inline name input */
    if (state->creating_file) {
        fb_fill_rect(sidebar_w + 200, 0, win->width - sidebar_w - 200, topbar_h, GLASS_TOPBAR);
        const char *label = state->renaming ? "Rename:" : "Name:";
        fb_draw_string_transparent(sidebar_w + 210, 12, label, GLASS_TEXT);
        gfx_draw_rounded_rect_filled(sidebar_w + 270, 5, 200, 30, 4, GLASS_BUTTON);
        char buf[64];
        strcpy(buf, state->new_file_name);
        if (win->focused) strcat(buf, "_");
        fb_draw_string_transparent(sidebar_w + 280, 12, buf, GLASS_TEXT);
    }
    
    /* File grid */
    int start_x = sidebar_w + 20;
    int start_y = topbar_h + 20 - state->scroll_y;
    int cx = start_x, cy = start_y;
    for (int i = 0; i < state->entry_count; i++) {
        if (cy + ICON_SIZE + 25 >= topbar_h && cy - 5 <= win->height) {
            if (i == state->selected_entry)
                gfx_draw_rounded_rect_filled(cx - 5, cy - 5, ICON_SIZE + 10, ICON_SIZE + 25, 5, GLASS_SELECT);
            int icon_id = (state->entries[i].type == VFS_DIRECTORY) ? ICON_FOLDER : ICON_FILE;
            icon_draw(cx, cy, icon_id);
            char name[32];
            strncpy(name, state->entries[i].name, 14);
            name[14] = '\0';
            if (strlen(state->entries[i].name) > 14) strcat(name, "..");
            fb_draw_string_transparent(cx - 5, cy + ICON_SIZE + 5, name, GLASS_TEXT);
        }
        cx += 120;
        if (cx + 120 > win->width) { cx = start_x; cy += 80; }
    }

    /* Scrollbar */
    int sb_x = win->width - 25;
    fb_fill_rect(sb_x, topbar_h, 25, win->height - topbar_h, GLASS_SIDEBAR);
    gfx_draw_rounded_rect_filled(sb_x + 2, topbar_h + 5,   21, 30, 4, GLASS_BUTTON);
    fb_draw_string_transparent(sb_x + 8, topbar_h + 12, "^", GLASS_TEXT);
    gfx_draw_rounded_rect_filled(sb_x + 2, win->height - 35, 21, 30, 4, GLASS_BUTTON);
    fb_draw_string_transparent(sb_x + 8, win->height - 28, "v", GLASS_TEXT);

    /* Context menu (drawn last, on top) */
    if (state->ctx_open) {
        int ctx_w = 130;
        int ctx_h = (state->ctx_target == -1) ? 125 : 155;
        int dx = state->ctx_x;
        int dy = state->ctx_y;
        if (dx + ctx_w > win->width)  dx = win->width  - ctx_w;
        if (dy + ctx_h > win->height) dy = win->height - ctx_h;

        gfx_draw_rounded_rect_filled(dx, dy, ctx_w, ctx_h, 5, GLASS_SIDEBAR);
        /* Slight border */
        fb_draw_rect(dx,   dy,   ctx_w,   ctx_h,   GLASS_HOVER);

        int iy = dy + 8;
        if (state->ctx_target != -1) {
            fb_draw_string_transparent(dx + 12, iy, "Open",   GLASS_TEXT); iy += 30;
            fb_draw_string_transparent(dx + 12, iy, "Copy",   GLASS_TEXT); iy += 30;
            fb_draw_string_transparent(dx + 12, iy, "Cut",    GLASS_TEXT); iy += 30;
            fb_draw_string_transparent(dx + 12, iy, "Rename", GLASS_TEXT); iy += 30;
            fb_draw_string_transparent(dx + 12, iy, "Delete", GLASS_ERROR);
        } else {
            fb_draw_string_transparent(dx + 12, iy, "New Folder", GLASS_TEXT); iy += 30;
            fb_draw_string_transparent(dx + 12, iy, "New File",   GLASS_TEXT); iy += 30;
            color_t pc = (state->clipboard_path[0] != '\0') ? GLASS_TEXT : GLASS_TEXT_MUTED;
            fb_draw_string_transparent(dx + 12, iy, "Paste",   pc);                    iy += 30;
            fb_draw_string_transparent(dx + 12, iy, "Refresh", GLASS_TEXT);
        }
    }
}

/* ─── mouse ───────────────────────────────────────────────── */

static void fm_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    struct fm_state *state = (struct fm_state *)win->user_data;
    int sidebar_w = 140;
    int topbar_h  = 40;

    /* Ignore pure hover (no click, no context menu) */
    if (!left && !right && !state->ctx_open) {
        /* Still handle drag-scroll even on hover */
        if (mouse_left_pressed() && mx > sidebar_w && mx < win->width - 25 && my > topbar_h) {
            if (!state->dragging) {
                state->dragging = true;
                state->last_my  = my;
            } else {
                int dy2 = state->last_my - my;
                if (dy2 != 0) {
                    state->scroll_y += dy2;
                    state->last_my   = my;
                    win->needs_redraw = true;
                }
            }
        } else {
            state->dragging = false;
        }
        return;
    }

    /* ── Context menu intercept ───────────────────────────── */
    if (state->ctx_open) {
        int ctx_w = 130;
        int ctx_h = (state->ctx_target == -1) ? 125 : 155;
        int dx = state->ctx_x;
        int dy = state->ctx_y;
        if (dx + ctx_w > win->width)  dx = win->width  - ctx_w;
        if (dy + ctx_h > win->height) dy = win->height - ctx_h;

        if (left || right) {
            if (left && mx >= dx && mx <= dx + ctx_w && my >= dy && my <= dy + ctx_h) {
                int item = (my - (dy + 8)) / 30;
                if (state->ctx_target != -1) {
                    /* file / folder actions */
                    char target[256];
                    strcpy(target, state->current_path);
                    if (strcmp(target, "/") != 0) strcat(target, "/");
                    strcat(target, state->entries[state->ctx_target].name);

                    if (item == 0) { /* Open */
                        if (state->entries[state->ctx_target].type == VFS_DIRECTORY)
                            fm_goto(state, target);
                        else
                            open_file_smart(target);
                    } else if (item == 1) { /* Copy */
                        strcpy(state->clipboard_path, target);
                        state->clipboard_is_cut = false;
                    } else if (item == 2) { /* Cut */
                        strcpy(state->clipboard_path, target);
                        state->clipboard_is_cut = true;
                    } else if (item == 3) { /* Rename */
                        state->creating_file = true;
                        state->renaming = true;
                        state->new_file_pos = strlen(state->entries[state->ctx_target].name);
                        strcpy(state->new_file_name, state->entries[state->ctx_target].name);
                        state->selected_entry = state->ctx_target;
                    } else if (item == 4) { /* Delete */
                        vfs_delete(target);
                        refresh_dir(state);
                    }
                } else {
                    /* empty-space actions */
                    if (item == 0) { /* New Folder */
                        state->creating_file = true;
                        state->creating_dir  = true;
                        state->renaming      = false;
                        state->new_file_pos  = 0;
                        state->new_file_name[0] = '\0';
                    } else if (item == 1) { /* New File */
                        state->creating_file = true;
                        state->creating_dir  = false;
                        state->renaming      = false;
                        state->new_file_pos  = 0;
                        state->new_file_name[0] = '\0';
                    } else if (item == 2) { /* Paste */
                        if (state->clipboard_path[0] != '\0') {
                            char dest[256];
                            strcpy(dest, state->current_path);
                            if (strcmp(dest, "/") != 0) strcat(dest, "/");
                            char *fn = strrchr(state->clipboard_path, '/');
                            if (fn) strcat(dest, fn + 1);

                            struct vfs_node *src = vfs_open(state->clipboard_path);
                            if (src) {
                                if (src->type == VFS_DIRECTORY) {
                                    vfs_create(dest, VFS_DIRECTORY);
                                } else {
                                    struct vfs_node *dst = vfs_create(dest, VFS_FILE);
                                    if (dst && src->size > 0) {
                                        void *buf = kmalloc(src->size);
                                        if (buf) {
                                            int rd = vfs_read(src, buf, src->size, 0);
                                            if (rd > 0) vfs_write(dst, buf, rd);
                                            kfree(buf);
                                        }
                                    }
                                }
                                if (state->clipboard_is_cut) {
                                    vfs_delete(state->clipboard_path);
                                    state->clipboard_path[0] = '\0';
                                }
                            }
                            refresh_dir(state);
                        }
                    } else if (item == 3) { /* Refresh */
                        refresh_dir(state);
                    }
                }
            }
            state->ctx_open = false;
            win->needs_redraw = true;
            return;
        }
        /* any other event just closes the menu */
        return;
    }

    /* ── Scrollbar ────────────────────────────────────────── */
    int sb_x = win->width - 25;
    if (left && mx >= sb_x && my > topbar_h) {
        if (my >= topbar_h + 5 && my <= topbar_h + 35) {
            state->scroll_y -= 80;
            if (state->scroll_y < 0) state->scroll_y = 0;
            win->needs_redraw = true;
            return;
        }
        if (my >= win->height - 35 && my <= win->height - 5) {
            state->scroll_y += 80;
            win->needs_redraw = true;
            return;
        }
    }

    /* ── Drag-to-scroll ───────────────────────────────────── */
    if (mouse_left_pressed() && !left && mx > sidebar_w && mx < sb_x && my > topbar_h) {
        if (!state->dragging) {
            state->dragging = true;
            state->last_my  = my;
        } else {
            int dy = state->last_my - my;
            if (dy != 0) {
                state->scroll_y += dy;
                state->last_my   = my;
                win->needs_redraw = true;
            }
        }
    } else if (!mouse_left_pressed()) {
        state->dragging = false;
    }

    /* ── Sidebar shortcuts ────────────────────────────────── */
    if ((left || right) && mx < sidebar_w) {
        if (left) {
            if (my >= 45 && my <= 75)  { fm_goto(state, "/");    win->needs_redraw = true; }
            if (my >= 85 && my <= 115) { fm_goto(state, "/hd0"); win->needs_redraw = true; }
        }
        return;
    }

    /* ── Top toolbar ──────────────────────────────────────── */
    if (my < topbar_h && mx >= sidebar_w) {
        if (!left) return;

        /* Back */
        if (mx >= sidebar_w + 5 && mx <= sidebar_w + 35) {
            if (state->history_idx > 0) {
                strcpy(state->history[state->history_idx], state->current_path);
                state->history_idx--;
                strcpy(state->current_path, state->history[state->history_idx]);
                refresh_dir(state);
                win->needs_redraw = true;
            }
            return;
        }
        /* Forward */
        if (mx >= sidebar_w + 40 && mx <= sidebar_w + 70) {
            if (state->history_idx < state->history_max) {
                strcpy(state->history[state->history_idx], state->current_path);
                state->history_idx++;
                strcpy(state->current_path, state->history[state->history_idx]);
                refresh_dir(state);
                win->needs_redraw = true;
            }
            return;
        }
        /* Path bar → go up */
        if (mx >= sidebar_w + 70 && mx <= sidebar_w + 270) {
            if (strcmp(state->current_path, "/") != 0) {
                char new_path[256];
                strcpy(new_path, state->current_path);
                char *ls = strrchr(new_path, '/');
                if (ls && ls != new_path) *ls = '\0';
                else strcpy(new_path, "/");
                fm_goto(state, new_path);
                win->needs_redraw = true;
            }
            return;
        }
        /* Delete */
        int btn_x = win->width - 100;
        if (state->selected_entry != -1) {
            if (mx >= btn_x && mx <= btn_x + 90) {
                char target[256];
                strcpy(target, state->current_path);
                if (strcmp(target, "/") != 0) strcat(target, "/");
                strcat(target, state->entries[state->selected_entry].name);
                vfs_delete(target);
                refresh_dir(state);
                win->needs_redraw = true;
                return;
            }
            btn_x -= 100;
        }
        /* + Folder */
        if (mx >= btn_x && mx <= btn_x + 90) {
            state->creating_file = true; state->creating_dir = true;
            state->renaming = false; state->new_file_pos = 0; state->new_file_name[0] = '\0';
            win->needs_redraw = true; return;
        }
        btn_x -= 100;
        /* + File */
        if (mx >= btn_x && mx <= btn_x + 90) {
            state->creating_file = true; state->creating_dir = false;
            state->renaming = false; state->new_file_pos = 0; state->new_file_name[0] = '\0';
            win->needs_redraw = true; return;
        }
        return;
    }

    /* ── Grid ─────────────────────────────────────────────── */
    int start_x = sidebar_w + 20;
    int start_y = topbar_h + 20 - state->scroll_y;
    int cx = start_x, cy = start_y;
    bool hit = false;

    for (int i = 0; i < state->entry_count; i++) {
        if (mx >= cx - 5 && mx < cx + ICON_SIZE + 10 &&
            my >= cy - 5 && my < cy + ICON_SIZE + 25) {
            hit = true;

            /* right-click → context menu on this item */
            if (right) {
                state->ctx_open   = true;
                state->ctx_x      = mx;
                state->ctx_y      = my;
                state->ctx_target = i;
                win->needs_redraw = true;
                return;
            }

            /* left-click */
            if (left) {
                if (state->selected_entry == i) {
                    /* double-click open */
                    if (state->entries[i].type == VFS_DIRECTORY) {
                        char np[256];
                        strcpy(np, state->current_path);
                        if (strcmp(np, "/") != 0) strcat(np, "/");
                        strcat(np, state->entries[i].name);
                        fm_goto(state, np);
                    } else {
                        char tp[256];
                        strcpy(tp, state->current_path);
                        if (strcmp(tp, "/") != 0) strcat(tp, "/");
                        strcat(tp, state->entries[i].name);
                        open_file_smart(tp);
                        state->selected_entry = -1;
                    }
                } else {
                    state->selected_entry = i;
                }
                win->needs_redraw = true;
            }
            return;
        }
        cx += 120;
        if (cx + 120 > win->width) { cx = start_x; cy += 80; }
    }

    /* Click on empty grid space */
    if (!hit) {
        if (left) {
            state->selected_entry = -1;
            win->needs_redraw = true;
        } else if (right) {
            state->ctx_open   = true;
            state->ctx_x      = mx;
            state->ctx_y      = my;
            state->ctx_target = -1;
            win->needs_redraw = true;
        }
    }
}

/* ─── keyboard ────────────────────────────────────────────── */

static void fm_on_key(struct window *win, char key, uint8_t scancode) {
    (void)scancode;
    struct fm_state *state = (struct fm_state *)win->user_data;
    if (!state->creating_file) return;

    if (key == '\b') {
        if (state->new_file_pos > 0) {
            state->new_file_pos--;
            state->new_file_name[state->new_file_pos] = '\0';
            win->needs_redraw = true;
        }
    } else if (key == '\n') {
        if (state->new_file_pos > 0) {
            char target[256];
            strcpy(target, state->current_path);
            if (strcmp(target, "/") != 0) strcat(target, "/");

            if (state->renaming && state->selected_entry != -1) {
                char old_target[256];
                strcpy(old_target, target);
                strcat(old_target, state->entries[state->selected_entry].name);
                strcat(target, state->new_file_name);
                int type = state->entries[state->selected_entry].type;
                vfs_delete(old_target);
                vfs_create(target, type);
            } else {
                strcat(target, state->new_file_name);
                vfs_create(target, state->creating_dir ? VFS_DIRECTORY : VFS_FILE);
            }
            refresh_dir(state);
        }
        state->creating_file = false;
        state->renaming      = false;
        win->needs_redraw    = true;
    } else if (key >= 32 && key <= 126) {
        if (state->new_file_pos < 30) {
            state->new_file_name[state->new_file_pos++] = key;
            state->new_file_name[state->new_file_pos]   = '\0';
            win->needs_redraw = true;
        }
    }
}

/* ─── close ───────────────────────────────────────────────── */

static void fm_on_close(struct window *win) {
    if (win->user_data) kfree(win->user_data);
}

/* ─── open ────────────────────────────────────────────────── */

struct window *file_manager_open_path(const char *path) {
    /* If File Manager is already open, focus it and navigate */
    struct window *win = wm_find_window_by_title("File Manager");
    if (win && win->user_data) {
        struct fm_state *state = (struct fm_state *)win->user_data;
        fm_goto(state, path ? path : "/");
        wm_focus_window(win);
        win->needs_redraw = true;
        return win;
    }

    win = wm_create_window("File Manager", 100, 100, 700, 450);
    if (!win) return NULL;
    
    struct fm_state *state = kcalloc(1, sizeof(struct fm_state));
    if (!state) { wm_destroy_window(win); return NULL; }
    
    strcpy(state->current_path, path ? path : "/");
    state->selected_entry   = -1;
    state->ctx_open         = false;
    state->ctx_target       = -1;
    state->clipboard_path[0] = '\0';
    state->renaming         = false;
    refresh_dir(state);
    
    win->user_data = state;
    win->on_draw   = fm_draw;
    win->on_mouse  = fm_on_mouse;
    win->on_key    = fm_on_key;
    win->on_close  = fm_on_close;
    
    return win;
}

struct window *file_manager_open(void) {
    return file_manager_open_path("/");
}
