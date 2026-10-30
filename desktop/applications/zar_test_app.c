/*
 * Zarqaa OS 0.1 - Zargraphics Architecture Test App
 * Creator: Abdelhak Chabbi & AI
 */

#include "libzargraphics/zargraphics.h"
#include "gui/compositor.h" /* For direct layer access before IPC is built */

zar_surface_t test_surf = NULL;
int anim_y = 100;
int anim_dir = 1;

void zar_test_app_init(void) {
    zar_graphics_init();
    
    test_surf = zar_create_surface(300, 200);
    if (!test_surf) return;
    
    /* Draw a beautiful modern window background */
    zar_draw_rect(test_surf, 0, 0, 300, 200, 0xFF1E1E2E); /* Dark theme bg */
    zar_draw_rect(test_surf, 0, 0, 300, 30, 0xFF11111B);  /* Title bar */
    zar_draw_rect(test_surf, 0, 30, 300, 2, 0xFF89B4FA);  /* Blue accent line */
    
    /* Add to compositor directly (bypass WM for test)
     * Place it high z-index (e.g. 99) so it floats over old desktop */
    compositor_add_surface((surface_t *)test_surf, 500, anim_y, 99, 230); // 230 alpha (translucent)
}

void zar_test_app_update(void) {
    if (!test_surf) return;
    
    static int tick_counter = 0;
    tick_counter++;
    if (tick_counter < 3) return; /* Only update every 3rd tick (~33 FPS) */
    tick_counter = 0;
    
    /* Simple bounce animation to prove the architecture is fast */
    anim_y += anim_dir * 2;
    if (anim_y > 400) anim_dir = -1;
    if (anim_y < 100) anim_dir = 1;
    
    /* Notice how we don't redraw the window! We just tell the compositor its new position.
     * The compositor merges this with the screen independently. 
     * This proves surface separation! */
    compositor_update_surface((surface_t *)test_surf, 500, anim_y, 99, 230);
}
