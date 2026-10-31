/* Auto-generated Zarqaa OS Wallpaper Thumbnails */
#ifndef GUI_WALLPAPER_THUMBNAILS_H
#define GUI_WALLPAPER_THUMBNAILS_H

#include "zarqaa/types.h"

#define WALLPAPER_THUMB_W 96
#define WALLPAPER_THUMB_H 54
#define NUM_WALLPAPERS 10

struct wallpaper_info {
    const char *title;
    const char *path;
    const uint32_t *thumb_pixels;
    int thumb_w;
    int thumb_h;
};

extern const struct wallpaper_info g_wallpapers[NUM_WALLPAPERS];
int wallpaper_get_count(void);
const struct wallpaper_info *wallpaper_get_info(int index);

#endif /* GUI_WALLPAPER_THUMBNAILS_H */
