#include "desktop/taskbar/widgets.h"
#include "zarqaa/string.h"
#include "zarqaa/types.h"
#include "gui/graphics.h"
#include "desktop/taskbar.h"
#include "drivers/gpu_renderer.h"

static taskbar_widget_entry_t g_widgets[MAX_WIDGETS];
static int g_widget_count = 0;

void taskbar_widgets_init(void) {
    g_widget_count = 0;
    for (int i = 0; i < MAX_WIDGETS; i++) {
        g_widgets[i].visible = false;
        g_widgets[i].priority = 0;
    }
}

void taskbar_widget_register(int priority, const taskbar_widget_t *widget) {
    if (g_widget_count >= MAX_WIDGETS || !widget) return;
    
    // Find insertion point (sort by priority ascending, so higher priority is drawn further left)
    // Wait, macOS draws from right to left.
    // If we draw from right to left:
    // lowest priority -> drawn first (furthest right).
    // highest priority -> drawn last (furthest left).
    
    int insert_idx = g_widget_count;
    for (int i = 0; i < g_widget_count; i++) {
        if (priority < g_widgets[i].priority) {
            insert_idx = i;
            break;
        }
    }
    
    // Shift elements right
    for (int i = g_widget_count; i > insert_idx; i--) {
        g_widgets[i] = g_widgets[i - 1];
    }
    
    g_widgets[insert_idx].visible = true;
    g_widgets[insert_idx].priority = priority;
    g_widgets[insert_idx].widget = *widget;
    g_widget_count++;
}

void taskbar_widgets_draw(int max_x, int y) {
    int current_x = max_x;
    
    // Draw widgets from index 0 to g_widget_count - 1
    for (int i = 0; i < g_widget_count; i++) {
        if (!g_widgets[i].visible) continue;
        
        taskbar_widget_t *w = &g_widgets[i].widget;
        if (w->get_width && w->draw) {
            int width = w->get_width();
            current_x -= width;
            w->draw(current_x, y);
            
            // Add spacing between widgets
            current_x -= 10;
        }
    }
}

bool taskbar_widgets_handle_click(int mx, int my) {
    int screen_w = (gpu_renderer_surface() ? gpu_renderer_surface()->width : 0);
    int current_x = screen_w - 10; // Same max_x as in taskbar_draw (assumed)
    
    // Y-bounds check (assuming topbar height is TOPBAR_HEIGHT)
    if (my < 0 || my > TOPBAR_HEIGHT) return false;

    for (int i = 0; i < g_widget_count; i++) {
        if (!g_widgets[i].visible) continue;
        
        taskbar_widget_t *w = &g_widgets[i].widget;
        if (w->get_width) {
            int width = w->get_width();
            current_x -= width;
            
            if (mx >= current_x && mx <= current_x + width) {
                if (w->on_click) {
                    w->on_click(mx, my);
                    return true;
                }
            }
            
            current_x -= 10;
        }
    }
    
    return false;
}
