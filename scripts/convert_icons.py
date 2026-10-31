import os
from PIL import Image

def generate_icons():
    icons_dir = "desktop/Icons"
    if not os.path.exists(icons_dir):
        print(f"Directory {icons_dir} not found!")
        return

    icon_files = {
        "browser": ("browser_64x64.png", "APP_HTML_VIEWER"),
        "terminal": ("terminal_64x64.png", "APP_TERMINAL"),
        "files": ("files_64x64.png", "APP_FILE_MANAGER"),
        "notepad": ("notepad_64x64.png", "APP_TEXT_EDITOR"),
        "studio": ("studio_64x64.png", "APP_PAINT"),
        "calculator": ("calculator_64x64.png", "APP_CALCULATOR"),
        "python_ide": ("python_ide_64x64.png", "APP_PYTHON_IDE"),
        "task_mgr": ("task_mgr_64x64.png", "APP_TASK_MANAGER"),
        "gallery": ("gallery_64x64.png", "APP_IMAGE_VIEWER"),
        "settings": ("settings_64x64.png", "APP_SETTINGS"),
        "boot_log": ("boot_log_64x64.png", "APP_BOOT_LOGGER"),
        "devices": ("devices_64x64.png", "APP_DEVICE_MANAGER"),
        "benchmark": ("benchmark_64x64.png", "APP_BENCHMARK"),
        "snake": ("snake_64x64.png", "APP_SNAKE"),
        "power": ("power_64x64.png", "APP_POWER")
    }

    # Sizes
    DOCK_SIZE = 40
    MENU_SIZE = 24

    out_h = "include/gui/bitmap_icons.h"
    out_c = "gui/bitmap_icons.c"

    with open(out_h, "w") as hf, open(out_c, "w") as cf:
        hf.write("/* Auto-generated Zarqaa OS Icon Bitmaps */\n")
        hf.write("#ifndef GUI_BITMAP_ICONS_H\n")
        hf.write("#define GUI_BITMAP_ICONS_H\n\n")
        hf.write("#include \"zarqaa/types.h\"\n\n")
        hf.write(f"#define ICON_DOCK_SIZE {DOCK_SIZE}\n")
        hf.write(f"#define ICON_MENU_SIZE {MENU_SIZE}\n\n")

        cf.write("/* Auto-generated Zarqaa OS Icon Bitmaps */\n")
        cf.write("#include \"gui/bitmap_icons.h\"\n")
        cf.write("#include \"drivers/framebuffer.h\"\n")
        cf.write("#include \"desktop/desktop.h\"\n\n")

        for key, (fname, app_id) in icon_files.items():
            fpath = os.path.join(icons_dir, fname)
            if not os.path.exists(fpath):
                print(f"Warning: {fpath} not found!")
                continue

            img = Image.open(fpath).convert("RGBA")

            # 1. Dock version (40x40)
            img_dock = img.resize((DOCK_SIZE, DOCK_SIZE), Image.Resampling.LANCZOS)
            hf.write(f"extern const uint32_t icon_dock_{key}[{DOCK_SIZE * DOCK_SIZE}];\n")
            cf.write(f"const uint32_t icon_dock_{key}[{DOCK_SIZE * DOCK_SIZE}] = {{\n    ")
            pix_dock = img_dock.load()
            count = 0
            for y in range(DOCK_SIZE):
                for x in range(DOCK_SIZE):
                    r, g, b, a = pix_dock[x, y]
                    val = (a << 24) | (r << 16) | (g << 8) | b
                    cf.write(f"0x{val:08X}, ")
                    count += 1
                    if count % 8 == 0:
                        cf.write("\n    ")
            cf.write("\n};\n\n")

            # 2. Menu version (24x24)
            img_menu = img.resize((MENU_SIZE, MENU_SIZE), Image.Resampling.LANCZOS)
            hf.write(f"extern const uint32_t icon_menu_{key}[{MENU_SIZE * MENU_SIZE}];\n")
            cf.write(f"const uint32_t icon_menu_{key}[{MENU_SIZE * MENU_SIZE}] = {{\n    ")
            pix_menu = img_menu.load()
            count = 0
            for y in range(MENU_SIZE):
                for x in range(MENU_SIZE):
                    r, g, b, a = pix_menu[x, y]
                    val = (a << 24) | (r << 16) | (g << 8) | b
                    cf.write(f"0x{val:08X}, ")
                    count += 1
                    if count % 8 == 0:
                        cf.write("\n    ")
            cf.write("\n};\n\n")

            print(f"Processed icon: {key} ({fname})")

        # Functions declarations
        hf.write("\nvoid gfx_draw_bitmap_rgba(int x, int y, const uint32_t *data, int w, int h);\n")
        hf.write("void gfx_draw_bitmap_rgba_centered(int cx, int cy, const uint32_t *data, int w, int h);\n")
        hf.write("const uint32_t *icon_get_dock_bitmap(int app_id);\n")
        hf.write("const uint32_t *icon_get_menu_bitmap(int app_id);\n")
        hf.write("\n#endif /* GUI_BITMAP_ICONS_H */\n")

        # Implement drawing function with true alpha blending
        cf.write("""
void gfx_draw_bitmap_rgba(int x, int y, const uint32_t *data, int w, int h) {
    if (!data) return;
    int scr_w = fb_get_width();
    int scr_h = fb_get_height();

    for (int r = 0; r < h; r++) {
        int py = y + r;
        if (py < 0 || py >= scr_h) continue;

        for (int c = 0; c < w; c++) {
            int px = x + c;
            if (px < 0 || px >= scr_w) continue;

            uint32_t fg = data[r * w + c];
            uint8_t alpha = (fg >> 24) & 0xFF;
            if (alpha == 0) continue;

            if (alpha == 255) {
                fb_putpixel(px, py, fg & 0x00FFFFFF);
            } else {
                uint32_t bg = fb_getpixel(px, py);
                uint8_t fg_r = (fg >> 16) & 0xFF;
                uint8_t fg_g = (fg >> 8) & 0xFF;
                uint8_t fg_b = fg & 0xFF;

                uint8_t bg_r = (bg >> 16) & 0xFF;
                uint8_t bg_g = (bg >> 8) & 0xFF;
                uint8_t bg_b = bg & 0xFF;

                uint8_t out_r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
                uint8_t out_g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
                uint8_t out_b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;

                fb_putpixel(px, py, (out_r << 16) | (out_g << 8) | out_b);
            }
        }
    }
}

void gfx_draw_bitmap_rgba_centered(int cx, int cy, const uint32_t *data, int w, int h) {
    gfx_draw_bitmap_rgba(cx - w / 2, cy - h / 2, data, w, h);
}

const uint32_t *icon_get_dock_bitmap(int app_id) {
    switch (app_id) {
        case APP_HTML_VIEWER:   return icon_dock_browser;
        case APP_TERMINAL:      return icon_dock_terminal;
        case APP_FILE_MANAGER:  return icon_dock_files;
        case APP_TEXT_EDITOR:   return icon_dock_notepad;
        case APP_PAINT:         return icon_dock_studio;
        case APP_CALCULATOR:    return icon_dock_calculator;
        case APP_PYTHON_IDE:    return icon_dock_python_ide;
        case APP_TASK_MANAGER:  return icon_dock_task_mgr;
        case APP_IMAGE_VIEWER:  return icon_dock_gallery;
        case APP_SETTINGS:      return icon_dock_settings;
        case APP_BOOT_LOGGER:   return icon_dock_boot_log;
        case APP_DEVICE_MANAGER:return icon_dock_devices;
        case APP_BENCHMARK:     return icon_dock_benchmark;
        case APP_SNAKE:         return icon_dock_snake;
        case APP_POWER:         return icon_dock_power;
        default:                return NULL;
    }
}

const uint32_t *icon_get_menu_bitmap(int app_id) {
    switch (app_id) {
        case APP_HTML_VIEWER:   return icon_menu_browser;
        case APP_TERMINAL:      return icon_menu_terminal;
        case APP_FILE_MANAGER:  return icon_menu_files;
        case APP_TEXT_EDITOR:   return icon_menu_notepad;
        case APP_PAINT:         return icon_menu_studio;
        case APP_CALCULATOR:    return icon_menu_calculator;
        case APP_PYTHON_IDE:    return icon_menu_python_ide;
        case APP_TASK_MANAGER:  return icon_menu_task_mgr;
        case APP_IMAGE_VIEWER:  return icon_menu_gallery;
        case APP_SETTINGS:      return icon_menu_settings;
        case APP_BOOT_LOGGER:   return icon_menu_boot_log;
        case APP_DEVICE_MANAGER:return icon_menu_devices;
        case APP_BENCHMARK:     return icon_menu_benchmark;
        case APP_SNAKE:         return icon_menu_snake;
        case APP_POWER:         return icon_menu_power;
        default:                return NULL;
    }
}
""")

    print("Generation complete!")

if __name__ == "__main__":
    generate_icons()
