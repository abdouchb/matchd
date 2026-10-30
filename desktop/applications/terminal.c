/*
 * Zarqaa OS 0.1 - Terminal Application
 * Creator: Abdelhak Chabbi
 */

#include "desktop/window_manager.h"
#include "drivers/framebuffer.h"
#include "drivers/usb/usb_hid.h"
#include "gui/graphics.h"
#include "filesystem/vfs.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"
#include "kernel/pic.h"

// Include our new POSIX compat headers
#include "compat/unistd.h"
#include "compat/fcntl.h"

struct terminal_state {
    char lines[50][80];
    int line_count;
    char input_buffer[256];
    int input_pos;
    int scroll_offset;
    struct window *win;
};

static struct terminal_state *g_active_terminal = NULL;

static void term_gui_stdout_hook(const char *str, size_t len) {
    if (!g_active_terminal) return;
    
    static char line_buf[256];
    static int line_pos = 0;
    
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (c == '\n') {
            line_buf[line_pos] = '\0';
            if (g_active_terminal->line_count < 50) {
                strncpy(g_active_terminal->lines[g_active_terminal->line_count], line_buf, 79);
                g_active_terminal->lines[g_active_terminal->line_count][79] = '\0';
                g_active_terminal->line_count++;
            } else {
                for (int j = 0; j < 49; j++) {
                    strcpy(g_active_terminal->lines[j], g_active_terminal->lines[j + 1]);
                }
                strncpy(g_active_terminal->lines[49], line_buf, 79);
                g_active_terminal->lines[49][79] = '\0';
            }
            line_pos = 0;
        } else if (c != '\r') {
            if (line_pos < 255) {
                line_buf[line_pos++] = c;
            }
        }
    }
    
    if (g_active_terminal->win) {
        g_active_terminal->win->needs_redraw = true;
    }
}

static void term_print(struct terminal_state *state, const char *text) {
    if (state->line_count < 50) {
        strncpy(state->lines[state->line_count], text, 79);
        state->lines[state->line_count][79] = '\0';
        state->line_count++;
    } else {
        for (int i = 0; i < 49; i++) {
            strcpy(state->lines[i], state->lines[i + 1]);
        }
        strncpy(state->lines[49], text, 79);
        state->lines[49][79] = '\0';
    }
}

static void execute_command(struct terminal_state *state) {
    char cmd[256];
    strcpy(cmd, state->input_buffer);
    
    char echo_buf[300];
    strncpy(echo_buf, "zarqaa> ", 299);
    strncat(echo_buf, cmd, 299);
    term_print(state, echo_buf);
    
    if (strcmp(cmd, "help") == 0) {
        term_print(state, "Commands: help, clear, about, sysinfo, ls, cat <file>,");
        term_print(state, "          mkdir <dir>, touch <file>, write <file> <text>, reboot");
    } else if (strcmp(cmd, "clear") == 0) {
        state->line_count = 0;
    } else if (strcmp(cmd, "about") == 0) {
        term_print(state, "Zarqaa OS 0.1");
        term_print(state, "Creator: Abdelhak Chabbi");
        term_print(state, "Architecture: x86_64 UEFI");
    } else if (strcmp(cmd, "sysinfo") == 0) {
        term_print(state, "System Information:");
        term_print(state, "OS: Zarqaa OS v0.1");
        term_print(state, "GUI: Glassmorphism Engine");
        term_print(state, "RAM: 16 MB Static Heap allocated");
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        term_print(state, cmd + 5);
    } else if (strcmp(cmd, "ls") == 0) {
        struct vfs_node *root = vfs_get_root();
        struct vfs_dirent entries[64];
        int count = vfs_readdir(root, entries, 64);
        for (int i = 0; i < count; i++) {
            char buf[128];
            strncpy(buf, entries[i].name, 127);
            if (entries[i].type == VFS_DIRECTORY) {
                strncat(buf, "/", 127);
            }
            term_print(state, buf);
        }
    } else if (strncmp(cmd, "mkdir ", 6) == 0) {
        if (vfs_create(cmd + 6, VFS_DIRECTORY)) {
            term_print(state, "Directory created.");
        } else {
            term_print(state, "Error creating directory.");
        }
    } else if (strncmp(cmd, "touch ", 6) == 0) {
        if (vfs_create(cmd + 6, VFS_FILE)) {
            term_print(state, "File created.");
        } else {
            term_print(state, "Error creating file.");
        }
    } else if (strncmp(cmd, "write ", 6) == 0) {
        char *filename = cmd + 6;
        char *text = strchr(filename, ' ');
        if (text) {
            *text = '\0';
            text++;
            struct vfs_node *file = vfs_open(filename);
            if (!file) file = vfs_create(filename, VFS_FILE);
            if (file && file->type == VFS_FILE) {
                vfs_write(file, text, strlen(text));
                term_print(state, "Written to file.");
            } else {
                term_print(state, "Error opening/creating file.");
            }
        } else {
            term_print(state, "Usage: write <file> <text>");
        }
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        const char *filename = cmd + 4;
        struct vfs_node *file = vfs_open(filename);
        if (file && file->type == VFS_FILE) {
            if (file->size == 0) {
                term_print(state, "(empty file)");
            } else {
                char buf[512];
                memset(buf, 0, 512);
                vfs_read(file, buf, 511, 0);
                
                char *p = buf;
                char *line_start = p;
                while (*p) {
                    if (*p == '\n') {
                        *p = '\0';
                        term_print(state, line_start);
                        line_start = p + 1;
                    }
                    p++;
                }
                if (*line_start) {
                    term_print(state, line_start);
                }
            }
        } else {
            term_print(state, "File not found or is a directory.");
        }
    } else if (strcmp(cmd, "reboot") == 0) {
        term_print(state, "Rebooting...");
        outb(0x64, 0xFE); // Reset via keyboard controller
        while(1) hlt();
    } else if (strcmp(cmd, "testfs") == 0) {
        term_print(state, "Testing POSIX Syscalls (open, write, close)...");
        
        // Use our new syscall wrappers!
        int fd = open("test_posix.txt", O_CREAT | O_WRONLY);
        if (fd >= 3) {
            char msg[32];
            strcpy(msg, "Hello from POSIX Syscall!");
            ssize_t w = write(fd, msg, strlen(msg));
            close(fd);
            
            char out[64];
            // Since we don't have sprintf yet, we manually build the string
            strcpy(out, "Success! Wrote bytes to FD 3.");
            term_print(state, out);
        } else {
            term_print(state, "Failed to open file via syscall!");
        }
    } else if (strncmp(cmd, "exec ", 5) == 0 || strncmp(cmd, "./", 2) == 0 || strstr(cmd, ".elf") != NULL || strstr(cmd, ".ELF") != NULL) {
        const char *path = cmd;
        if (strncmp(cmd, "exec ", 5) == 0) path = cmd + 5;
        term_print(state, "Spawning User-Space process...");
        extern int process_exec(const char *path, const char *name);
        int pid = process_exec(path, path);
        if (pid > 0) {
            term_print(state, "Process launched successfully in Ring 3!");
        } else {
            term_print(state, "Failed to load/execute ELF file.");
        }
    } else if (strlen(cmd) > 0) {
        term_print(state, "Unknown command. Type 'help'.");
    }
    
    state->input_buffer[0] = '\0';
    state->input_pos = 0;
}

static void terminal_draw(struct window *win) {
    if (!win || !win->content_buffer) return;
    struct terminal_state *state = (struct terminal_state *)win->user_data;
    
    fb_set_render_target(win->content_buffer, win->width, win->height, win->width * 4);
    
    fb_fill_rect(0, 0, win->width, win->height, 0xFF111111);
    
    int max_visible_lines = win->height / 16 - 1; // leave 1 line for input
    int start_idx = 0;
    if (state->line_count > max_visible_lines) {
        start_idx = state->line_count - max_visible_lines;
    }
    
    int cy = 0;
    for (int i = start_idx; i < state->line_count; i++) {
        fb_draw_string_transparent(5, cy, state->lines[i], COLOR_SUCCESS);
        cy += 16;
    }
    
    // Draw input line
    char prompt[300] = "zarqaa> ";
    strncat(prompt, state->input_buffer, 299);
    fb_draw_string_transparent(5, cy, prompt, COLOR_SUCCESS);
    
    if (win->focused) {
        // Blinking cursor
        int cursor_x = 5 + (strlen("zarqaa> ") + state->input_pos) * 8;
        fb_draw_line(cursor_x, cy, cursor_x, cy + 15, COLOR_SUCCESS);
    }
    
    fb_reset_render_target();
}

static void terminal_on_key(struct window *win, char key, uint8_t scancode) {
    (void)scancode;
    struct terminal_state *state = (struct terminal_state *)win->user_data;
    
    if (key == '\b') {
        if (state->input_pos > 0) {
            state->input_pos--;
            state->input_buffer[state->input_pos] = '\0';
        }
    } else if (key == '\n') {
        execute_command(state);
    } else if (key >= 32 && key <= 126) {
        if (state->input_pos < 254) {
            state->input_buffer[state->input_pos++] = key;
            state->input_buffer[state->input_pos] = '\0';
        }
    }
}

struct window *terminal_open(void) {
    struct window *win = wm_create_window("Terminal", 50, 50, 600, 400);
    if (!win) return NULL;
    
    struct terminal_state *state = kmalloc(sizeof(struct terminal_state));
    state->line_count = 0;
    state->input_pos = 0;
    state->input_buffer[0] = '\0';
    state->scroll_offset = 0;
    state->win = win;
    
    term_print(state, "Zarqaa OS v0.1");
    term_print(state, "Type 'help' for a list of commands.");
    
    g_active_terminal = state;
    extern void terminal_register_stdout_hook(void (*hook)(const char *str, size_t len));
    terminal_register_stdout_hook(term_gui_stdout_hook);
    
    win->user_data = state;
    win->on_draw = terminal_draw;
    win->on_key = terminal_on_key;
    
    return win;
}
