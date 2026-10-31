/*
 * Zarqaa OS 0.1 - Python IDE & Mini Engine
 * Creator: Abdelhak Chabbi & AI
 *
 * Features:
 * - Code Editor (Top half)
 * - Output Console (Bottom half)
 * - Mini-Python Interpreter (Variables, Math, Print)
 */

#include "desktop/window_manager.h"
#include "drivers/framebuffer.h"
#include "gui/graphics.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"
#include "drivers/pit.h"
#include "drivers/font.h"
#include "filesystem/vfs.h"

#define IDE_MARGIN_LEFT 10
#define IDE_TOP_BAR 40

/* =========================================================
 *  Mini-Python Engine Core
 * ========================================================= */

#define MAX_VARS 64
#define PY_TYPE_INT 0
#define PY_TYPE_STR 1

struct py_var {
    char name[32];
    int type;
    int i_val;
    char s_val[128];
};

struct py_env {
    struct py_var vars[MAX_VARS];
    int var_count;
    char output[4096];
    int out_len;
    bool error;
};

static void py_print_out(struct py_env *env, const char *str) {
    int slen = strlen(str);
    if (env->out_len + slen < 4095) {
        strcpy(env->output + env->out_len, str);
        env->out_len += slen;
    }
}

static void py_print_int(struct py_env *env, int val) {
    char buf[32];
    itoa(val, buf, 10);
    py_print_out(env, buf);
}

static struct py_var *py_get_var(struct py_env *env, const char *name) {
    for (int i = 0; i < env->var_count; i++) {
        if (strcmp(env->vars[i].name, name) == 0) return &env->vars[i];
    }
    return NULL;
}

static struct py_var *py_set_var(struct py_env *env, const char *name) {
    struct py_var *v = py_get_var(env, name);
    if (v) return v;
    if (env->var_count >= MAX_VARS) return NULL;
    v = &env->vars[env->var_count++];
    strcpy(v->name, name);
    v->type = PY_TYPE_INT;
    v->i_val = 0;
    return v;
}

/* Very simple expression evaluator for integers */
static int py_eval_expr(struct py_env *env, const char **cursor) {
    const char *p = *cursor;
    while (*p == ' ') p++;
    
    int val = 0;
    
    if (*p >= '0' && *p <= '9') {
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
    } else if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
        char name[32];
        int i = 0;
        while (((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_') && i < 31) {
            name[i++] = *p++;
        }
        name[i] = '\0';
        struct py_var *v = py_get_var(env, name);
        if (v && v->type == PY_TYPE_INT) val = v->i_val;
    }
    
    while (*p == ' ') p++;
    
    if (*p == '+') {
        p++;
        val += py_eval_expr(env, &p);
    } else if (*p == '-') {
        p++;
        val -= py_eval_expr(env, &p);
    } else if (*p == '*') {
        p++;
        val *= py_eval_expr(env, &p);
    }
    
    *cursor = p;
    return val;
}

static void py_run_line(struct py_env *env, const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '#') return;
    
    /* Parse print() */
    if (strncmp(line, "print(", 6) == 0) {
        const char *p = line + 6;
        while (*p == ' ') p++;
        
        if (*p == '"') {
            p++;
            char buf[256];
            int i = 0;
            while (*p && *p != '"' && i < 255) {
                buf[i++] = *p++;
            }
            buf[i] = '\0';
            py_print_out(env, buf);
            py_print_out(env, "\n");
        } else {
            int val = py_eval_expr(env, &p);
            py_print_int(env, val);
            py_print_out(env, "\n");
        }
        return;
    }
    
    /* Parse Assignment: var = expr */
    const char *eq = strchr(line, '=');
    if (eq) {
        char name[32];
        int i = 0;
        const char *p = line;
        while (p < eq && *p != ' ' && i < 31) {
            name[i++] = *p++;
        }
        name[i] = '\0';
        
        struct py_var *v = py_set_var(env, name);
        if (v) {
            const char *expr = eq + 1;
            while (*expr == ' ') expr++;
            
            if (*expr == '"') {
                v->type = PY_TYPE_STR;
                expr++;
                int j = 0;
                while (*expr && *expr != '"' && j < 127) {
                    v->s_val[j++] = *expr++;
                }
                v->s_val[j] = '\0';
            } else {
                v->type = PY_TYPE_INT;
                v->i_val = py_eval_expr(env, &expr);
            }
        }
    }
}

static void py_execute_script(const char *code, char *output_buffer) {
    struct py_env env;
    memset(&env, 0, sizeof(env));
    
    char line[256];
    int line_len = 0;
    
    for (int i = 0; code[i]; i++) {
        if (code[i] == '\n' || line_len >= 255) {
            line[line_len] = '\0';
            py_run_line(&env, line);
            line_len = 0;
        } else {
            line[line_len++] = code[i];
        }
    }
    if (line_len > 0) {
        line[line_len] = '\0';
        py_run_line(&env, line);
    }
    
    strcpy(output_buffer, env.output);
}

/* =========================================================
 *  IDE UI Core
 * ========================================================= */

struct ide_state {
    char text_buffer[8192];
    int text_length;
    int cursor_pos;
    
    char output_buffer[8192];
    
    int editor_scroll;
    int output_scroll;
    
    bool is_running;
};

static void ide_draw(struct window *win) {
    struct ide_state *state = (struct ide_state *)win->user_data;
    
    int split_y = win->height - 200; // Output console takes bottom 200px
    if (split_y < IDE_TOP_BAR + 50) split_y = IDE_TOP_BAR + 50;
    
    /* 1. Draw Editor Area (Top) */
    fb_fill_rect(0, IDE_TOP_BAR, win->width, split_y - IDE_TOP_BAR, 0xFF1E1E2E);
    
    int cx = IDE_MARGIN_LEFT;
    int cy = IDE_TOP_BAR + 10 - state->editor_scroll;
    int cursor_draw_x = cx;
    int cursor_draw_y = cy;
    
    for (int i = 0; i <= state->text_length; i++) {
        if (i == state->cursor_pos) {
            cursor_draw_x = cx;
            cursor_draw_y = cy;
        }
        if (i == state->text_length) break;
        
        char c = state->text_buffer[i];
        if (c == '\n') {
            cy += 16;
            cx = IDE_MARGIN_LEFT;
        } else {
            if (cy >= IDE_TOP_BAR && cy < split_y - 16) {
                char buf[2] = {c, '\0'};
                fb_draw_string_transparent(cx, cy, buf, 0xFFCDD6F4);
            }
            cx += 8;
            if (cx > win->width - 20) {
                cy += 16;
                cx = IDE_MARGIN_LEFT;
            }
        }
    }
    
    /* Draw Cursor */
    if (win->focused && (pit_get_ticks() % 100) < 50) {
        if (cursor_draw_y >= IDE_TOP_BAR && cursor_draw_y < split_y - 16) {
            fb_fill_rect(cursor_draw_x, cursor_draw_y, 2, 16, 0xFFF38BA8);
        }
    }
    
    /* 2. Draw Output Console (Bottom) */
    fb_fill_rect(0, split_y, win->width, win->height - split_y, 0xFF11111B);
    fb_draw_line(0, split_y, win->width, split_y, 0xFF45475A);
    fb_draw_string_transparent(10, split_y + 5, "Console Output:", 0xFFA6E3A1);
    
    int out_cx = 10;
    int out_cy = split_y + 25 - state->output_scroll;
    
    for (int i = 0; state->output_buffer[i]; i++) {
        char c = state->output_buffer[i];
        if (c == '\n') {
            out_cy += 16;
            out_cx = 10;
        } else {
            if (out_cy >= split_y + 25 && out_cy < win->height - 16) {
                char buf[2] = {c, '\0'};
                fb_draw_string_transparent(out_cx, out_cy, buf, 0xFFBAC2DE);
            }
            out_cx += 8;
            if (out_cx > win->width - 20) {
                out_cy += 16;
                out_cx = 10;
            }
        }
    }
    
    /* 3. Draw Top Bar */
    gfx_draw_glass_panel(0, 0, win->width, IDE_TOP_BAR, 0, 0xFF181825, 0xFF45475A, 255);
    
    /* Run Button */
    gfx_draw_rounded_rect_filled(10, 8, 60, 24, 6, 0xFFA6E3A1);
    fb_draw_string_transparent(25, 12, "Run", 0xFF11111B);
}

static void ide_on_key(struct window *win, char key, uint8_t scancode) {
    struct ide_state *state = (struct ide_state *)win->user_data;
    
    if (key == '\b') {
        if (state->cursor_pos > 0) {
            for (int i = state->cursor_pos - 1; i < state->text_length; i++) {
                state->text_buffer[i] = state->text_buffer[i + 1];
            }
            state->cursor_pos--;
            state->text_length--;
        }
    } else if ((key >= 32 && key <= 126) || key == '\n') {
        if (state->text_length < 8191) {
            for (int i = state->text_length; i > state->cursor_pos; i--) {
                state->text_buffer[i] = state->text_buffer[i - 1];
            }
            state->text_buffer[state->cursor_pos] = key;
            state->cursor_pos++;
            state->text_length++;
            state->text_buffer[state->text_length] = '\0';
        }
    } else if (key == '\0') {
        if (scancode == 0x4B) { // Left
            if (state->cursor_pos > 0) state->cursor_pos--;
        } else if (scancode == 0x4D) { // Right
            if (state->cursor_pos < state->text_length) state->cursor_pos++;
        }
    }
    win->needs_redraw = true;
}

static void ide_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    if (!left) return;
    struct ide_state *state = (struct ide_state *)win->user_data;
    
    if (my >= 8 && my <= 32 && mx >= 10 && mx <= 70) {
        /* Clicked Run! */
        strcpy(state->output_buffer, "Running...\n");
        py_execute_script(state->text_buffer, state->output_buffer);
        win->needs_redraw = true;
    }
}

struct window *python_ide_open_file(const char *path) {
    struct window *win = wm_create_window("Python IDE", 150, 100, 600, 500);
    if (!win) return NULL;
    
    struct ide_state *state = kcalloc(1, sizeof(struct ide_state));
    
    if (path) {
        struct vfs_node *file = vfs_open(path);
        if (file && file->type == VFS_FILE && file->size < 8192) {
            vfs_read(file, state->text_buffer, file->size, 0);
            state->text_length = file->size;
            state->text_buffer[state->text_length] = '\0';
        }
    } else {
        const char *default_code = "x = 10\n"
                                   "y = 20\n"
                                   "print(\"Hello from Zarqaa Python!\")\n"
                                   "print(\"x + y =\")\n"
                                   "print(x + y)\n";
        
        strcpy(state->text_buffer, default_code);
        state->text_length = strlen(default_code);
    }
    
    state->cursor_pos = state->text_length;
    
    win->user_data = state;
    win->on_draw = ide_draw;
    win->on_key = ide_on_key;
    win->on_mouse = ide_on_mouse;
    
    return win;
}

struct window *python_ide_open(void) {
    return python_ide_open_file(NULL);
}
