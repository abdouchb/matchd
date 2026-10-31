/*
 * Zarqaa OS 0.1 - Advanced Task Manager
 * Creator: Abdelhak Chabbi
 *
 * Premium HUD-style system monitor with:
 * - CPU usage gauge (animated arc)
 * - RAM usage bar (live from PMM)
 * - Heap usage bar (live from heap)
 * - Uptime counter
 * - CPU frequency & voltage estimates
 * - Process list with per-process status
 */

#include "desktop/window_manager.h"
#include "kernel/scheduler.h"
#include "kernel/heap.h"
#include "kernel/pmm.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "drivers/pit.h"
#include "kernel/hardware_sensors.h"
#include "zarqaa/string.h"

/* ── Theme Colors ── */
#define TM_BG         0xFF0A0E1A
#define TM_PANEL      0xFF111827
#define TM_BORDER     0xFF1E293B
#define TM_CYAN       0xFF06D6E7
#define TM_GREEN      0xFF22C55E
#define TM_ORANGE     0xFFF97316
#define TM_RED        0xFFEF4444
#define TM_BLUE       0xFF3B82F6
#define TM_PURPLE     0xFF8B5CF6
#define TM_TEXT_W     0xFFE2E8F0
#define TM_TEXT_DIM   0xFF64748B
#define TM_TEXT_MUTED 0xFF475569

struct task_manager_state {
    int scroll_y;
    int tab;           /* 0 = Overview, 1 = Processes */
    int cpu_usage;     /* Real CPU usage 0-100 */
    char cpu_brand[50];
    bool is_vm;
    uint32_t last_refresh_ticks;
};

static void get_cpu_info(struct task_manager_state *st) {
    uint32_t regs[4];
    memset(st->cpu_brand, 0, sizeof(st->cpu_brand));
    
    // Check extended CPUID support
    __asm__ volatile ("cpuid" : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3]) : "a"(0x80000000));
    if (regs[0] >= 0x80000004) {
        for (int i = 0; i < 3; i++) {
            __asm__ volatile (
                "cpuid"
                : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                : "a"(0x80000002 + i)
            );
            memcpy(st->cpu_brand + i * 16, regs, 16);
        }
    } else {
        strcpy(st->cpu_brand, "Unknown CPU");
    }
    
    // Cleanup leading spaces
    int shift = 0;
    while (st->cpu_brand[shift] == ' ') shift++;
    if (shift > 0) {
        memmove(st->cpu_brand, st->cpu_brand + shift, 50 - shift);
    }

    // Check hypervisor bit
    __asm__ volatile ("cpuid" : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3]) : "a"(1));
    st->is_vm = (regs[2] & (1 << 31)) != 0;
}

/* ── Helper: Format bytes to human-readable ── */
static void format_bytes(uint64_t bytes, char *out) {
    out[0] = '\0';
    if (bytes >= 1024ULL * 1024 * 1024) {
        uint64_t gb_x10 = (bytes * 10) / (1024ULL * 1024 * 1024);
        char whole[16], frac[4];
        itoa((int)(gb_x10 / 10), whole, 10);
        itoa((int)(gb_x10 % 10), frac, 10);
        strcpy(out, whole);
        strcat(out, ".");
        strcat(out, frac);
        strcat(out, " GB");
    } else if (bytes >= 1024ULL * 1024) {
        char tmp[16];
        itoa((int)(bytes / (1024ULL * 1024)), tmp, 10);
        strcpy(out, tmp);
        strcat(out, " MB");
    } else if (bytes >= 1024) {
        char tmp[16];
        itoa((int)(bytes / 1024), tmp, 10);
        strcpy(out, tmp);
        strcat(out, " KB");
    } else {
        char tmp[16];
        itoa((int)bytes, tmp, 10);
        strcpy(out, tmp);
        strcat(out, " B");
    }
}

/* ── Helper: Draw a semi-circular gauge ── */
static void draw_gauge(int cx, int cy, int r, int percent, color_t fg, color_t bg) {
    int r_inner = r - 2;
    int r_outer = r + 2;
    
    for (int y = -r_outer; y <= 0; y++) {
        for (int x = -r_outer; x <= r_outer; x++) {
            int d2 = x*x + y*y;
            if (d2 >= r_inner*r_inner && d2 <= r_outer*r_outer) {
                int ax = x < 0 ? -x : x;
                int ay = y < 0 ? -y : y;
                
                int base_angle;
                if (ax == 0) base_angle = 90;
                else if (ay == 0) base_angle = 0;
                else if (ax >= ay) base_angle = (45 * ay) / ax;
                else base_angle = 90 - (45 * ax) / ay;
                
                int angle;
                if (x < 0) {
                    angle = 180 + base_angle;
                } else {
                    angle = 360 - base_angle;
                }
                
                int fill_angle = 180 + (percent * 180) / 100;
                if (angle <= fill_angle) {
                    fb_putpixel(cx + x, cy + y, fg);
                } else {
                    fb_putpixel(cx + x, cy + y, bg);
                }
            }
        }
    }
}

/* ── Helper: Draw usage bar ── */
static void draw_usage_bar(int x, int y, int w, int h, int percent,
                            color_t fg, color_t bg_bar) {
    gfx_draw_rounded_rect_filled(x, y, w, h, 4, bg_bar);
    int fill_w = (w * percent) / 100;
    if (fill_w > 0) {
        gfx_draw_rounded_rect_filled(x, y, fill_w, h, 4, fg);
    }
}

/* ── Helper: Draw stat card ── */
static void draw_stat_card(int x, int y, int w, int h,
                            const char *label, const char *value,
                            const char *unit, color_t accent) {
    gfx_draw_rounded_rect_filled(x, y, w, h, 8, TM_PANEL);
    gfx_draw_rounded_rect(x, y, w, h, 8, TM_BORDER);
    
    /* Accent line at top */
    fb_fill_rect(x + 10, y + 2, w - 20, 2, accent);
    
    fb_draw_string_transparent(x + 10, y + 12, label, TM_TEXT_DIM);
    fb_draw_string_transparent(x + 10, y + 30, value, TM_TEXT_W);
    if (unit && unit[0]) {
        int vlen = strlen(value);
        fb_draw_string_transparent(x + 10 + vlen * 8 + 4, y + 30, unit, TM_TEXT_DIM);
    }
}

/* ════════════════════════════════════════════
 *   OVERVIEW TAB
 * ════════════════════════════════════════════ */
static void draw_overview(struct window *win, struct task_manager_state *st) {
    int w = win->width;
    
    /* ── CPU Section ── */
    gfx_draw_rounded_rect_filled(10, 10, w / 2 - 15, 160, 8, TM_PANEL);
    gfx_draw_rounded_rect(10, 10, w / 2 - 15, 160, 8, TM_BORDER);
    fb_draw_string_transparent(20, 18, "CPU", TM_CYAN);
    
    /* CPU Usage Gauge */
    int gauge_cx = 10 + (w / 2 - 15) / 2;
    int gauge_cy = 120;
    draw_gauge(gauge_cx, gauge_cy, 50, st->cpu_usage, TM_CYAN, TM_BORDER);
    
    /* Percent text in center of gauge */
    char cpu_str[8];
    itoa(st->cpu_usage, cpu_str, 10);
    strcat(cpu_str, "%");
    int cpu_len = strlen(cpu_str);
    fb_draw_string_transparent(gauge_cx - cpu_len * 4, gauge_cy - 8, cpu_str, TM_TEXT_W);
    
    /* CPU Info */
    // Ensure the string isn't too long for the card
    char display_brand[32];
    strncpy(display_brand, st->cpu_brand, 30);
    display_brand[30] = '\0';
    fb_draw_string_transparent(20, 145, display_brand, TM_TEXT_DIM);
    
    /* ── RAM Section ── */
    int rx = w / 2 + 5;
    int rw = w / 2 - 15;
    gfx_draw_rounded_rect_filled(rx, 10, rw, 160, 8, TM_PANEL);
    gfx_draw_rounded_rect(rx, 10, rw, 160, 8, TM_BORDER);
    fb_draw_string_transparent(rx + 10, 18, "MEMORY", TM_GREEN);
    
    uint64_t total_ram = pmm_get_total();
    uint64_t used_ram = pmm_get_used();
    uint64_t free_ram = (used_ram <= total_ram) ? (total_ram - used_ram) : 0;
    int ram_pct = 0;
    if (total_ram > 0) ram_pct = (int)((used_ram * 100) / total_ram);
    if (ram_pct > 100) ram_pct = 100;
    if (ram_pct < 0) ram_pct = 0;
    
    /* RAM Usage Ring */
    int ram_ring_cx = rx + rw / 2;
    int ram_ring_cy = 95;
    gfx_draw_progress_ring(ram_ring_cx, ram_ring_cy, 35, ram_pct, TM_BORDER, TM_GREEN);
    char ram_pct_str[8];
    itoa(ram_pct, ram_pct_str, 10);
    strcat(ram_pct_str, "%");
    int rp_len = strlen(ram_pct_str);
    fb_draw_string_transparent(ram_ring_cx - rp_len * 4, ram_ring_cy - 6, ram_pct_str, TM_TEXT_W);
    
    /* RAM Numbers */
    char used_str[32] = {0}, total_str[32] = {0};
    format_bytes(used_ram, used_str);
    format_bytes(total_ram, total_str);
    
    fb_draw_string_transparent(rx + 10, 140, "Used: ", TM_TEXT_DIM);
    fb_draw_string_transparent(rx + 58, 140, used_str, TM_TEXT_W);
    fb_draw_string_transparent(rx + 10, 155, "Total:", TM_TEXT_DIM);
    fb_draw_string_transparent(rx + 58, 155, total_str, TM_TEXT_W);
    
    /* ── Bottom Stat Cards ── */
    int card_y = 180;
    int card_w = (w - 50) / 4;
    int card_h = 60;
    
    /* Card 1: Uptime */
    uint32_t uptime = pit_get_seconds();
    char up_min[8], up_sec[8], up_str[20] = {0};
    itoa(uptime / 60, up_min, 10);
    itoa(uptime % 60, up_sec, 10);
    strcpy(up_str, up_min);
    strcat(up_str, "m ");
    strcat(up_str, up_sec);
    strcat(up_str, "s");
    draw_stat_card(10, card_y, card_w, card_h, "UPTIME", up_str, "", TM_CYAN);
    
    /* Card 2: Fan Speed (RPM) */
    uint32_t fan_rpm = get_cpu_fan_rpm();
    char fan_str[12];
    itoa(fan_rpm, fan_str, 10);
    draw_stat_card(10 + card_w + 10, card_y, card_w, card_h, "FAN SPEED", fan_str, "RPM", TM_BLUE);
    
    /* Card 3: Voltage (V) */
    uint32_t mv = get_cpu_voltage_mv();
    char volt_str[16];
    char v_whole[8], v_frac[4];
    itoa(mv / 1000, v_whole, 10);
    itoa((mv % 1000) / 100, v_frac, 10);
    strcpy(volt_str, v_whole);
    strcat(volt_str, ".");
    strcat(volt_str, v_frac);
    draw_stat_card(10 + (card_w + 10) * 2, card_y, card_w, card_h, "VOLTAGE", volt_str, "V", TM_ORANGE);
    
    /* Card 4: CPU Temperature (MSR / DTS) */
    uint32_t temp = get_cpu_temp_celsius();
    char temp_str[8];
    itoa(temp, temp_str, 10);
    draw_stat_card(10 + (card_w + 10) * 3, card_y, card_w, card_h, "CPU TEMP", temp_str, "C", TM_RED);
    
    /* ── Usage Bars ── */
    int bar_y = 255;
    
    /* CPU bar (Per-Core) */
    int cores = scheduler_get_core_count();
    if (cores < 1) cores = 1;
    
    char cpu_lbl[16];
    snprintf(cpu_lbl, sizeof(cpu_lbl), "CPU (%d)", cores);
    fb_draw_string_transparent(10, bar_y, cpu_lbl, TM_TEXT_DIM);
    
    int total_bar_w = w - 80;
    int gap = 4;
    int single_w = (total_bar_w - (cores - 1) * gap) / cores;
    if (single_w < 4) single_w = 4; // minimum width
    
    int current_x = 60;
    for (int i = 0; i < cores; i++) {
        int core_usage = scheduler_get_core_usage(i);
        draw_usage_bar(current_x, bar_y + 2, single_w, 12, core_usage, TM_CYAN, TM_BORDER);
        current_x += single_w + gap;
    }
    
    /* RAM bar */
    fb_draw_string_transparent(10, bar_y + 22, "RAM", TM_TEXT_DIM);
    draw_usage_bar(60, bar_y + 24, w - 80, 12, ram_pct, TM_GREEN, TM_BORDER);
    
    /* Heap bar */
    size_t total_heap = heap_get_total();
    size_t used_heap = heap_get_used();
    int heap_pct = (total_heap > 0) ? (int)((used_heap * 100) / total_heap) : 0;
    if (heap_pct < 1 && used_heap > 0) heap_pct = 1;
    fb_draw_string_transparent(10, bar_y + 44, "HEAP", TM_TEXT_DIM);
    draw_usage_bar(60, bar_y + 46, w - 80, 12, heap_pct, TM_PURPLE, TM_BORDER);
    
    /* Free RAM */
    char free_str[32] = {0};
    format_bytes(free_ram, free_str);
    fb_draw_string_transparent(10, bar_y + 68, "Free RAM: ", TM_TEXT_DIM);
    fb_draw_string_transparent(90, bar_y + 68, free_str, TM_GREEN);
}

/* ════════════════════════════════════════════
 *   PROCESSES TAB 
 * ════════════════════════════════════════════ */
static void draw_processes(struct window *win) {
    int w = win->width;
    struct task_manager_state *st = (struct task_manager_state *)win->user_data;
    (void)st;
    
    /* Table Header */
    gfx_draw_rounded_rect_filled(10, 10, w - 20, 28, 6, TM_PANEL);
    fb_draw_string_transparent(20, 16, "PID", TM_CYAN);
    fb_draw_string_transparent(70, 16, "PROCESS", TM_CYAN);
    fb_draw_string_transparent(230, 16, "STATE", TM_CYAN);
    fb_draw_string_transparent(330, 16, "CPU", TM_CYAN);
    fb_draw_string_transparent(385, 16, "MEM", TM_CYAN);
    
    fb_draw_line(10, 42, w - 10, 42, TM_BORDER);
    
    /* Process Rows */
    int count = scheduler_get_task_count();
    int y = 48;
    
    for (int i = 0; i < count; i++) {
        if (y + 28 > win->height - 10) break;
        
        int id, state;
        char name[32];
        scheduler_get_task_info(i, &id, name, &state);
        
        /* Alternating row background */
        if (i % 2 == 0) {
            fb_fill_rect(10, y, w - 20, 26, 0xFF0F1629);
        }
        
        /* PID */
        char id_str[10];
        itoa(id, id_str, 10);
        fb_draw_string_transparent(20, y + 6, id_str, TM_TEXT_DIM);
        
        /* Name */
        fb_draw_string_transparent(70, y + 6, name, TM_TEXT_W);
        
        /* State with color */
        const char *state_str = "IDLE";
        color_t state_col = TM_TEXT_DIM;
        if (state == 1) { state_str = "READY";   state_col = TM_GREEN; }
        if (state == 2) { state_str = "RUNNING"; state_col = TM_CYAN; }
        if (state == 3) { state_str = "BLOCKED"; state_col = TM_ORANGE; }
        
        /* State badge */
        int badge_w = strlen(state_str) * 8 + 12;
        gfx_draw_rounded_rect_filled(225, y + 3, badge_w, 20, 4, 
            (state == 2) ? 0xFF0A2A35 : (state == 1) ? 0xFF0A2A1A : 0xFF2A1A0A);
        fb_draw_string_transparent(231, y + 6, state_str, state_col);
        
        /* CPU % */
        int task_cpu = scheduler_get_task_cpu_usage(i);
        char cpu_pct[12];
        itoa(task_cpu, cpu_pct, 10);
        strcat(cpu_pct, "%");
        fb_draw_string_transparent(330, y + 6, cpu_pct, TM_TEXT_DIM);
        
        /* Memory */
        fb_draw_string_transparent(385, y + 6, "16 KB", TM_TEXT_DIM);
        
        /* End Task Button (for user/app tasks PID > 1) */
        if (id > 1) {
            gfx_draw_rounded_rect_filled(w - 75, y + 3, 60, 20, 4, TM_RED);
            fb_draw_string_transparent(w - 65, y + 6, "End", TM_TEXT_W);
        }
        
        y += 28;
    }
}

/* ════════════════════════════════════════════
 *   DRAW & EVENT HANDLERS
 * ════════════════════════════════════════════ */
static void tm_on_draw(struct window *win) {
    struct task_manager_state *st = (struct task_manager_state *)win->user_data;
    if (!st) return;
    
    fb_set_render_target(win->content_buffer, win->width, win->height, win->width * 4);
    
    /* Window background */
    fb_fill_rect(0, 0, win->width, win->height, TM_BG);
    
    /* Tab Bar */
    int tab_w = 120;
    for (int t = 0; t < 2; t++) {
        int tx = 10 + t * (tab_w + 5);
        bool active = (st->tab == t);
        gfx_draw_rounded_rect_filled(tx, win->height - 35, tab_w, 28, 6,
            active ? TM_CYAN : TM_PANEL);
        const char *tab_name = (t == 0) ? "Overview" : "Processes";
        color_t tab_col = active ? TM_BG : TM_TEXT_DIM;
        fb_draw_string_transparent(tx + (tab_w - strlen(tab_name) * 8) / 2, win->height - 29, tab_name, tab_col);
    }
    
    /* Read real CPU usage from scheduler */
    int usage = scheduler_get_cpu_usage();
    st->cpu_usage = (st->cpu_usage * 3 + usage) / 4;
    if (st->cpu_usage < 1 && usage > 0) st->cpu_usage = usage;
    if (st->cpu_usage > 100) st->cpu_usage = 100;
    
    /* Draw active tab */
    if (st->tab == 0) {
        draw_overview(win, st);
    } else {
        draw_processes(win);
    }
    
    fb_reset_render_target();
}

static void tm_on_tick(struct window *win) {
    struct task_manager_state *st = (struct task_manager_state *)win->user_data;
    if (!st) return;
    
    /* Auto-refresh: 4Hz (every 15 frames or 250ms) */
    static int frame_counter = 0;
    frame_counter++;
    uint32_t now = pit_get_ticks();
    if ((now - st->last_refresh_ticks >= 250) || (frame_counter >= 15)) {
        st->last_refresh_ticks = now;
        frame_counter = 0;
        win->needs_redraw = true;
    }
}

static void tm_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    struct task_manager_state *st = (struct task_manager_state *)win->user_data;
    if (!st || !left) return;
    
    /* Tab click detection */
    int tab_w = 120;
    if (my >= win->height - 35 && my <= win->height - 7) {
        for (int t = 0; t < 2; t++) {
            int tx = 10 + t * (tab_w + 5);
            if (mx >= tx && mx <= tx + tab_w) {
                st->tab = t;
                win->needs_redraw = true;
                return;
            }
        }
    }
    
    /* Process End Task click detection */
    if (st->tab == 1) {
        int count = scheduler_get_task_count();
        int y = 48;
        for (int i = 0; i < count; i++) {
            if (y + 28 > win->height - 10) break;
            
            int id, state;
            scheduler_get_task_info(i, &id, NULL, &state);
            
            if (id > 1 && mx >= win->width - 75 && mx <= win->width - 15 && my >= y + 3 && my <= y + 23) {
                task_kill(id);
                win->needs_redraw = true;
                break;
            }
            y += 28;
        }
    }
}

static void tm_on_close(struct window *win) {
    if (win->user_data) {
        kfree(win->user_data);
    }
}

struct window *task_manager_open(void) {
    struct window *win = wm_create_window("Task Manager", 120, 60, 520, 390);
    if (!win) return NULL;

    struct task_manager_state *state = kcalloc(1, sizeof(struct task_manager_state));
    state->tab = 0;
    state->cpu_usage = 0;
    get_cpu_info(state);
    win->user_data = state;
    
    win->on_draw = tm_on_draw;
    win->on_tick = tm_on_tick;
    win->on_mouse = tm_on_mouse;
    win->on_close = tm_on_close;
    return win;
}
