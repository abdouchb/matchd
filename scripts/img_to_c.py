import sys
import os
from PIL import Image

def main():
    if len(sys.argv) != 5:
        print("Usage: img_to_c.py <input.jpg> <output_name> <width> <height>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_name = sys.argv[2]
    target_w = int(sys.argv[3])
    target_h = int(sys.argv[4])

    print(f"Loading {input_path}...")
    try:
        img = Image.open(input_path).convert("RGBA")
    except Exception as e:
        print(f"Error loading image: {e}")
        sys.exit(1)

    print(f"Resizing to {target_w}x{target_h}...")
    img = img.resize((target_w, target_h), Image.Resampling.LANCZOS)
    pixels = img.load()

    os.makedirs("include/gui", exist_ok=True)
    os.makedirs("gui", exist_ok=True)

    h_path = f"include/gui/{output_name}.h"
    c_path = f"gui/{output_name}.c"

    print(f"Writing {h_path}...")
    with open(h_path, "w") as hf:
        hf.write(f"/* Auto-generated from {os.path.basename(input_path)} */\n")
        hf.write(f"#ifndef GUI_{output_name.upper()}_H\n")
        hf.write(f"#define GUI_{output_name.upper()}_H\n\n")
        hf.write("#include \"zarqaa/types.h\"\n\n")
        hf.write(f"extern const uint32_t {output_name}[{target_w * target_h}];\n")
        hf.write(f"#define {output_name.upper()}_WIDTH {target_w}\n")
        hf.write(f"#define {output_name.upper()}_HEIGHT {target_h}\n\n")
        hf.write(f"#endif\n")

    print(f"Writing {c_path}...")
    with open(c_path, "w") as cf:
        cf.write(f"#include \"gui/{output_name}.h\"\n\n")
        cf.write(f"const uint32_t {output_name}[{target_w * target_h}] = {{\n    ")
        
        count = 0
        for y in range(target_h):
            for x in range(target_w):
                r, g, b, a = pixels[x, y]
                # Framebuffer format: ARGB or BGRA depending on system. Assuming standard 0xAARRGGBB
                val = (0xFF << 24) | (r << 16) | (g << 8) | b
                cf.write(f"0x{val:08X},")
                count += 1
                if count % 12 == 0:
                    cf.write("\n    ")
        cf.write("\n};\n")

    print("Done!")

if __name__ == "__main__":
    main()
