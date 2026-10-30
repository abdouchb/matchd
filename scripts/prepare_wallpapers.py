import os
from PIL import Image

WALLPAPERS = [
    ("Mountain_Minimalist_Artistic_Landscape.jpg", "mountain.jpg", "Mountain Peak"),
    ("Lucid_Origin_a_surreal_and_vibrant_cinematic_photo_featuring_s_2.jpg", "neon_oasis.jpg", "Neon Oasis"),
    ("Lucid_Origin_a_surreal_and_vibrant_cinematic_photograph_embrac_0.jpg", "aurora_peak.jpg", "Aurora Peak"),
    ("Lucid_Origin_a_surreal_and_vibrant_cinematic_photograph_embrac_1.jpg", "cyber_dusk.jpg", "Cyber Dusk"),
    ("Lucid_Origin_a_surreal_and_vibrant_cinematic_photograph_embrac_2.jpg", "cosmic_glow.jpg", "Cosmic Glow"),
    ("Lucid_Origin_a_surreal_and_vibrant_cinematic_photograph_embrac_3.jpg", "solar_drift.jpg", "Solar Drift"),
    ("Lucid_Origin_a_surreal_and_vibrant_cinematic_photograph_in_the_0.jpg", "synth_wave.jpg", "Synthwave"),
    ("Lucid_Origin_a_surreal_and_vibrant_cinematic_photograph_in_the_1.jpg", "future_city.jpg", "Future City"),
    ("Lucid_Origin_a_surreal_and_vibrant_cinematic_photograph_in_the_3.jpg", "neon_horizon.jpg", "Neon Horizon"),
    ("1153825.jpg", "anime_dream.jpg", "Anime Dream")
]

def main():
    src_dir = "desktop/images"
    dst_dir = "fat_root/wallpapers"
    os.makedirs(dst_dir, exist_ok=True)

    out_h = "include/gui/wallpaper_thumbnails.h"
    out_c = "gui/wallpaper_thumbnails.c"

    THUMB_W = 96
    THUMB_H = 54

    valid_wallpapers = []

    print("=== Processing Wallpapers ===")
    for src_name, dst_name, title in WALLPAPERS:
        src_path = os.path.join(src_dir, src_name)
        if not os.path.exists(src_path):
            print(f"Skipping missing: {src_path}")
            continue

        # Open and convert
        img = Image.open(src_path).convert("RGB")
        
        # Save optimized 1366x768 JPEG for fat_root
        img_opt = img.resize((1366, 768), Image.Resampling.LANCZOS)
        dst_path = os.path.join(dst_dir, dst_name)
        img_opt.save(dst_path, "JPEG", quality=85)
        print(f"Saved optimized wallpaper: {dst_path}")

        # Create Thumbnail 96x54
        img_thumb = img.resize((THUMB_W, THUMB_H), Image.Resampling.LANCZOS)
        valid_wallpapers.append((dst_name, title, img_thumb))

    print(f"Generating C headers for {len(valid_wallpapers)} wallpapers...")

    # Write Header
    with open(out_h, "w") as hf:
        hf.write("/* Auto-generated Zarqaa OS Wallpaper Thumbnails */\n")
        hf.write("#ifndef GUI_WALLPAPER_THUMBNAILS_H\n")
        hf.write("#define GUI_WALLPAPER_THUMBNAILS_H\n\n")
        hf.write("#include \"zarqaa/types.h\"\n\n")
        hf.write(f"#define WALLPAPER_THUMB_W {THUMB_W}\n")
        hf.write(f"#define WALLPAPER_THUMB_H {THUMB_H}\n")
        hf.write(f"#define NUM_WALLPAPERS {len(valid_wallpapers)}\n\n")
        hf.write("struct wallpaper_info {\n")
        hf.write("    const char *title;\n")
        hf.write("    const char *path;\n")
        hf.write("    const uint32_t *thumb_pixels;\n")
        hf.write("    int thumb_w;\n")
        hf.write("    int thumb_h;\n")
        hf.write("};\n\n")
        hf.write("extern const struct wallpaper_info g_wallpapers[NUM_WALLPAPERS];\n")
        hf.write("int wallpaper_get_count(void);\n")
        hf.write("const struct wallpaper_info *wallpaper_get_info(int index);\n\n")
        hf.write("#endif /* GUI_WALLPAPER_THUMBNAILS_H */\n")

    # Write C source
    with open(out_c, "w") as cf:
        cf.write("/* Auto-generated Zarqaa OS Wallpaper Thumbnails */\n")
        cf.write("#include \"gui/wallpaper_thumbnails.h\"\n\n")

        for idx, (dst_name, title, img_thumb) in enumerate(valid_wallpapers):
            cf.write(f"static const uint32_t thumb_pixels_{idx}[{THUMB_W * THUMB_H}] = {{\n    ")
            pix = img_thumb.load()
            count = 0
            for y in range(THUMB_H):
                for x in range(THUMB_W):
                    r, g, b = pix[x, y]
                    val = 0xFF000000 | (r << 16) | (g << 8) | b
                    cf.write(f"0x{val:08X}, ")
                    count += 1
                    if count % 8 == 0:
                        cf.write("\n    ")
            cf.write("\n};\n\n")

        cf.write("const struct wallpaper_info g_wallpapers[NUM_WALLPAPERS] = {\n")
        for idx, (dst_name, title, img_thumb) in enumerate(valid_wallpapers):
            vfs_path = f"/wallpapers/{dst_name}"
            cf.write(f"    {{\"{title}\", \"{vfs_path}\", thumb_pixels_{idx}, {THUMB_W}, {THUMB_H}}},\n")
        cf.write("};\n\n")

        cf.write("""int wallpaper_get_count(void) {
    return NUM_WALLPAPERS;
}

const struct wallpaper_info *wallpaper_get_info(int index) {
    if (index < 0 || index >= NUM_WALLPAPERS) return 0;
    return &g_wallpapers[index];
}
""")

    print("Wallpaper preparation complete!")

if __name__ == "__main__":
    main()
