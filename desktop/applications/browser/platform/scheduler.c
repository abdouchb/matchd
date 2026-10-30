#include "scheduler.h"

static bool pending_layout = true;
static bool pending_paint = true;

void browser_scheduler_init(void) {
    pending_layout = true;
    pending_paint = true;
}

void browser_scheduler_invalidate_layout(void) {
    pending_layout = true;
    pending_paint = true; // Layout changes always require repaint
}

void browser_scheduler_invalidate_paint(void) {
    pending_paint = true;
}

bool browser_scheduler_needs_layout(void) {
    return pending_layout;
}

bool browser_scheduler_needs_paint(void) {
    return pending_paint;
}

void browser_scheduler_clear_flags(void) {
    pending_layout = false;
    pending_paint = false;
}

void browser_scheduler_clear_layout_flag(void) {
    pending_layout = false;
}

void browser_scheduler_clear_paint_flag(void) {
    pending_paint = false;
}
