import os
from PIL import Image

def convert_icons_to_c(input_folder, output_h, output_c):
    if not os.path.exists(input_folder):
        print(f"Error: Folder '{input_folder}' not found!")
        return

    icons = [f for f in os.listdir(input_folder) if f.endswith('.png')]
    if not icons:
        print("No PNG files found in the folder.")
        return

    with open(output_h, 'w') as h_file, open(output_c, 'w') as c_file:
        h_file.write("/* Auto-generated Icon Bitmaps */\n")
        h_file.write("#ifndef BITMAP_ICONS_H\n#define BITMAP_ICONS_H\n\n")
        h_file.write("#include \"zarqaa/types.h\"\n\n")

        c_file.write("/* Auto-generated Icon Bitmaps */\n")
        c_file.write("#include \"gui/bitmap_icons.h\"\n\n")

        for icon in icons:
            name = icon.replace('.png', '')
            try:
                img = Image.open(os.path.join(input_folder, icon)).convert("RGBA")
                w, h = img.size
                
                # Optional: resize to 32x32 or 48x48 if they are too big
                # img = img.resize((48, 48), Image.Resampling.LANCZOS)
                # w, h = img.size

                h_file.write(f"extern const uint32_t icon_{name}[{w * h}];\n")
                h_file.write(f"#define ICON_{name.upper()}_W {w}\n")
                h_file.write(f"#define ICON_{name.upper()}_H {h}\n\n")

                c_file.write(f"const uint32_t icon_{name}[{w * h}] = {{\n    ")
                
                pixels = img.load()
                count = 0
                for y in range(h):
                    for x in range(w):
                        r, g, b, a = pixels[x, y]
                        # 32-bit ARGB format
                        val = (a << 24) | (r << 16) | (g << 8) | b
                        c_file.write(f"0x{val:08X}, ")
                        count += 1
                        if count % 8 == 0:
                            c_file.write("\n    ")
                
                c_file.write("\n};\n\n")
                print(f"Converted: {icon} ({w}x{h})")

            except Exception as e:
                print(f"Failed to convert {icon}: {e}")

        h_file.write("#endif\n")
    print(f"Successfully generated {output_h} and {output_c}")

if __name__ == "__main__":
    # Configure these paths based on where you extract the ZIP
    INPUT_DIR = "desktop/images/icons"
    OUT_H = "include/gui/bitmap_icons.h"
    OUT_C = "gui/bitmap_icons.c"
    
    convert_icons_to_c(INPUT_DIR, OUT_H, OUT_C)
