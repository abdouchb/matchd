import sys
from PIL import Image

def generate_wallpaper_c(img_path, w, h):
    print(f"Loading {img_path}...")
    img = Image.open(img_path).convert('RGB')
    print(f"Resizing to {w}x{h}...")
    img = img.resize((w, h), Image.Resampling.LANCZOS)
    
    pixels = list(img.getdata())
    
    print("Writing include/gui/wallpaper.h...")
    with open("include/gui/wallpaper.h", "w") as f:
        f.write("#ifndef GUI_WALLPAPER_H\n")
        f.write("#define GUI_WALLPAPER_H\n\n")
        f.write("#include \"zarqaa/types.h\"\n\n")
        f.write(f"extern const uint32_t wallpaper[{w * h}];\n")
        f.write(f"#define WALLPAPER_WIDTH {w}\n")
        f.write(f"#define WALLPAPER_HEIGHT {h}\n\n")
        f.write("#endif /* GUI_WALLPAPER_H */\n")
        
    print("Writing gui/wallpaper.c...")
    with open("gui/wallpaper.c", "w") as f:
        f.write("#include \"gui/wallpaper.h\"\n\n")
        f.write(f"const uint32_t wallpaper[{w * h}] = {{\n")
        
        line = "    "
        count = 0
        for r, g, b in pixels:
            # Format: 0xFFRRGGBB
            hex_val = f"0xFF{r:02X}{g:02X}{b:02X}"
            line += hex_val + ","
            count += 1
            if count % 12 == 0:
                f.write(line + "\n")
                line = "    "
        if line != "    ":
            f.write(line + "\n")
            
        f.write("};\n")
        
    print("Done!")

if __name__ == "__main__":
    generate_wallpaper_c("desktop/images/Mountain_Minimalist_Artistic_Landscape.jpg", 1366, 768)
