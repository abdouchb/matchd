import os
from PIL import Image

def generate_icons(img_path, output_h, output_c):
    try:
        img = Image.open(img_path).convert("RGBA")
    except Exception as e:
        print(f"Error opening image: {e}")
        return

    crops = {
        "home": (15, 100, 75, 155),
        "cube": (15, 165, 75, 220),
        "monitor": (15, 230, 75, 285),
        "speed": (15, 295, 75, 350),
        "chip": (15, 360, 75, 415),
        "database": (15, 425, 75, 480),
        "wifi": (15, 490, 75, 545),
        "monitoring": (15, 555, 75, 610),
        "settings": (15, 620, 75, 675),
        "user": (15, 685, 75, 740),
        "folder": (15, 750, 75, 805),

        "sys_overview": (340, 750, 420, 840),
        "sys_update": (445, 750, 525, 840),
        "sys_security": (570, 750, 650, 840),
        "sys_monitor": (700, 750, 780, 840),
        "sys_opt": (830, 750, 910, 840),
        "sys_clean": (950, 750, 1030, 840),
        "sys_power": (1070, 750, 1150, 840),
        "sys_person": (1190, 750, 1270, 840),
        "sys_apps": (1320, 750, 1400, 840),

        "top_search": (1350, 30, 1390, 70),
        "top_user": (1410, 30, 1450, 70),
        "top_bell": (1470, 30, 1510, 70),
        "top_settings": (1530, 30, 1570, 70),
        "top_power": (1590, 30, 1630, 70),
    }

    with open(output_h, 'w') as h_file, open(output_c, 'w') as c_file:
        h_file.write("/* Auto-generated Icon Bitmaps */\n")
        h_file.write("#ifndef BITMAP_ICONS_H\n#define BITMAP_ICONS_H\n\n")
        h_file.write("#include \"zarqaa/types.h\"\n\n")

        c_file.write("/* Auto-generated Icon Bitmaps */\n")
        c_file.write("#include \"gui/bitmap_icons.h\"\n\n")

        for name, box in crops.items():
            try:
                crop = img.crop(box)
                w, h = crop.size

                h_file.write(f"extern const uint32_t icon_{name}[{w * h}];\n")
                h_file.write(f"#define ICON_{name.upper()}_W {w}\n")
                h_file.write(f"#define ICON_{name.upper()}_H {h}\n\n")

                c_file.write(f"const uint32_t icon_{name}[{w * h}] = {{\n    ")
                
                pixels = crop.load()
                count = 0
                for y in range(h):
                    for x in range(w):
                        r, g, b, a = pixels[x, y]
                        # Pre-multiply alpha or just store ARGB
                        val = (a << 24) | (r << 16) | (g << 8) | b
                        c_file.write(f"0x{val:08X}, ")
                        count += 1
                        if count % 8 == 0:
                            c_file.write("\n    ")
                
                c_file.write("\n};\n\n")
                print(f"Generated: icon_{name} ({w}x{h})")

            except Exception as e:
                print(f"Failed to process {name}: {e}")

        h_file.write("#endif\n")
    print(f"Successfully generated {output_h} and {output_c}")

if __name__ == "__main__":
    import sys
    img_path = sys.argv[1]
    OUT_H = "include/gui/bitmap_icons.h"
    OUT_C = "gui/bitmap_icons.c"
    
    os.makedirs("include/gui", exist_ok=True)
    os.makedirs("gui", exist_ok=True)
    generate_icons(img_path, OUT_H, OUT_C)
