/*
 * Zarqaa OS - System Info ("About Zarqaa OS")
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "kernel/pmm.h"
#include "gui/zb_theme.h"

static void cpu_brand(char *out) {
    uint32_t r[12];
    uint32_t max;
    __asm__ volatile("cpuid" : "=a"(max) : "a"(0x80000000u) : "ebx", "ecx", "edx");
    out[0] = 0;
    if (max < 0x80000004u) { strcpy(out, "Unknown x86_64 CPU"); return; }
    for (uint32_t i = 0; i < 3; i++)
        __asm__ volatile("cpuid" : "=a"(r[i * 4]), "=b"(r[i * 4 + 1]), "=c"(r[i * 4 + 2]), "=d"(r[i * 4 + 3])
                         : "a"(0x80000002u + i));
    memcpy(out, r, 48);
    out[48] = 0;
    char *p = out;
    while (*p == ' ') p++;
    if (p != out) memmove(out, p, strlen(p) + 1);
}

static void row(int y, const char *k, const char *v) {
    fb_draw_string_transparent(24, y, k, ZB_DIM);
    fb_draw_string_transparent(150, y, v, ZB_TEXT);
}

static void bar(int y, uint64_t used, uint64_t total) {
    int w = 300;
    gfx_draw_rounded_rect_filled(24, y, w, 10, 5, ZB_PANEL_HI);
    if (total) {
        int f = (int)(used * w / total);
        if (f < 10) f = 10;
        if (f > w) f = w;
        gfx_draw_rounded_rect_filled(24, y, f, 10, 5, ZB_ACCENT);
    }
}

static void si_on_draw(struct window *win) {
    fb_fill_rect(0, 0, win->width, win->height, ZB_BG);
    gfx_draw_rounded_rect_filled(10, 10, win->width - 20, 76, 12, ZB_PANEL);
    gfx_draw_circle_filled(52, 48, 26, ZB_ACCENT);
    zb_text_center(26, 40, 52, "Z", 0xFFFFFFFF);
    fb_draw_string_transparent(96, 28, "Zarqaa OS", ZB_TEXT);
    fb_draw_string_transparent(96, 50, "Blue edition - x86_64", ZB_ACCENT_HI);

    char buf[64];
    int y = 104;
    cpu_brand(buf);
    row(y, "Processor", buf); y += 24;
    snprintf(buf, sizeof(buf), "%d x %d", fb_get_width(), fb_get_height());
    row(y, "Display", buf); y += 24;

    uint64_t pt = pmm_get_total() >> 20, pu = pmm_get_used() >> 20;
    snprintf(buf, sizeof(buf), "%d / %d MB", (int)pu, (int)pt);
    row(y, "Memory", buf); y += 20;
    bar(y, pu, pt); y += 22;

    uint64_t ht = heap_get_total() >> 10, hu = heap_get_used() >> 10;
    snprintf(buf, sizeof(buf), "%d / %d KB", (int)hu, (int)ht);
    row(y, "Kernel heap", buf); y += 20;
    bar(y, hu, ht); y += 24;

    uint32_t up = pit_get_seconds();
    snprintf(buf, sizeof(buf), "%dh %dm %ds", (int)(up / 3600), (int)((up / 60) % 60), (int)(up % 60));
    row(y, "Uptime", buf); y += 24;
    snprintf(buf, sizeof(buf), "%d", wm_get_window_count());
    row(y, "Open windows", buf); y += 24;
    row(y, "Theme", "Zarqaa Blue");
}

static void si_on_tick(struct window *win) {
    static uint64_t last;
    uint64_t now = pit_get_ticks();
    if (now - last >= 500) { last = now; win->needs_redraw = true; }
}

struct window *sysinfo_open(void) {
    struct window *win = wm_create_window("About Zarqaa OS", 320, 130, 360, 340);
    if (!win) return NULL;
    win->on_draw = si_on_draw;
    win->on_tick = si_on_tick;
    return win;
}
