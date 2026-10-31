import sys
from PIL import Image

def generate_header(image_path, output_path, array_name, target_w, target_h):
    try:
        img = Image.open(image_path).convert('RGBA')
        img = img.resize((target_w, target_h), Image.LANCZOS)
        
        with open(output_path, 'w') as f:
            f.write("/* Auto-generated Boot Splash Header */\n")
            f.write("#ifndef BOOT_SPLASH_H\n")
            f.write("#define BOOT_SPLASH_H\n\n")
            f.write("#include \"zarqaa/types.h\"\n\n")
            
            f.write(f"const uint32_t {array_name}[{target_w * target_h}] = {{\n")
            
            pixels = list(img.getdata())
            line = "    "
            for i, p in enumerate(pixels):
                r, g, b, a = p
                # ZarqaaOS color_t is AARRGGBB, where usually A=0xFF for opaque
                # But since the OS draws to the backbuffer and fb_putpixel handles it,
                # we just write raw 0xFFRRGGBB values.
                hex_val = f"0xFF{r:02X}{g:02X}{b:02X}"
                line += f"{hex_val},"
                
                if (i + 1) % 8 == 0:
                    f.write(line + "\n")
                    line = "    "
            
            if line.strip():
                f.write(line + "\n")
                
            f.write("};\n\n")
            f.write("#endif\n")
            
        print(f"Successfully generated {output_path}")
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    img_in = r"desktop\boot image\Zarqaa_boot.png"
    header_out = r"include\gui\boot_splash.h"
    generate_header(img_in, header_out, "boot_splash_img", 1366, 768)
