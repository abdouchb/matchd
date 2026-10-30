"""
Zarqaa OS — Smart SVG-to-C Asset Pipeline v2.0
Creator: Abdelhak Chabbi

Extracts individual SVG icons from the QuantumPeak OS HTML simulation,
renders each one at high quality via headless Chrome, and converts
them to C uint32_t arrays for use in the kernel framebuffer.
"""

import os
import sys
import time
import json
import tempfile
from selenium import webdriver
from selenium.webdriver.chrome.options import Options
from PIL import Image

# ══════════════════════════════════════════════════════════════
# Icon Definitions — extracted from quantumpeak_os_simulation.html
# ══════════════════════════════════════════════════════════════

SIDEBAR_ICONS = {
    "sidebar_home": {
        "svg": '<path stroke-linecap="round" stroke-linejoin="round" d="M3 12l2-2m0 0l7-7 7 7M5 10v10a1 1 0 001 1h3m10-11l2 2m-2-2v10a1 1 0 01-1 1h-3m-6 0a1 1 0 001-1v-4a1 1 0 011-1h2a1 1 0 011 1v4a1 1 0 001 1m-6 0h6"></path>',
        "color": "#00f0ff", "size": 32
    },
    "sidebar_cube": {
        "svg": '<path d="M20 7l-8-4-8 4m16 0l-8 4m8-4v10l-8 4m0-10L4 7m8 4v10M4 7v10l8 4" />',
        "color": "#80a0c4", "size": 32
    },
    "sidebar_monitor": {
        "svg": '<rect x="2" y="3" width="20" height="14" rx="2" ry="2" /><line x1="8" y1="21" x2="16" y2="21" /><line x1="12" y1="17" x2="12" y2="21" />',
        "color": "#80a0c4", "size": 32
    },
    "sidebar_dial": {
        "svg": '<circle cx="12" cy="12" r="10" /><path d="M12 12l4-4m-4 4a2 2 0 100-4 2 2 0 000 4z" /><path d="M12 2v2m0 16v2m10-10h-2M4 12H2" />',
        "color": "#80a0c4", "size": 32
    },
    "sidebar_chip": {
        "svg": '<rect x="4" y="4" width="16" height="16" rx="2" ry="2" /><rect x="9" y="9" width="6" height="6" /><line x1="9" y1="1" x2="9" y2="4" /><line x1="15" y1="1" x2="15" y2="4" /><line x1="9" y1="20" x2="9" y2="23" /><line x1="15" y1="20" x2="15" y2="23" /><line x1="20" y1="9" x2="23" y2="9" /><line x1="20" y1="14" x2="23" y2="14" /><line x1="1" y1="9" x2="4" y2="9" /><line x1="1" y1="14" x2="4" y2="14" />',
        "color": "#80a0c4", "size": 32
    },
    "sidebar_database": {
        "svg": '<ellipse cx="12" cy="5" rx="9" ry="3" /><path d="M21 12c0 1.66-4 3-9 3s-9-1.34-9-3" /><path d="M3 5v14c0 1.66 4 3 9 3s9-1.34 9-3V5" />',
        "color": "#80a0c4", "size": 32
    },
    "sidebar_wifi": {
        "svg": '<path d="M5 12.55a11 11 0 0114.08 0M1.42 9a16 16 0 0121.16 0M8.53 16.11a6 6 0 016.95 0M12 20h.01" />',
        "color": "#80a0c4", "size": 32
    },
    "sidebar_pulse": {
        "svg": '<path d="M22 12h-4l-3 9L9 3l-3 9H2" />',
        "color": "#80a0c4", "size": 32
    },
    "sidebar_settings": {
        "svg": '<circle cx="12" cy="12" r="3" /><path d="M19.4 15a1.65 1.65 0 00.33 1.82l.06.06a2 2 0 010 2.83 2 2 0 01-2.83 0l-.06-.06a1.65 1.65 0 00-1.82-.33 1.65 1.65 0 00-1 1.51V21a2 2 0 01-2 2 2 2 0 01-2-2v-.09A1.65 1.65 0 009 19.4a1.65 1.65 0 00-1.82.33l-.06.06a2 2 0 01-2.83 0 2 2 0 010-2.83l.06-.06a1.65 1.65 0 00.33-1.82 1.65 1.65 0 00-1.51-1H3a2 2 0 01-2-2 2 2 0 012-2h.09A1.65 1.65 0 004.6 9a1.65 1.65 0 00-.33-1.82l-.06-.06a2 2 0 010-2.83 2 2 0 012.83 0l.06.06a1.65 1.65 0 001.82.33H9a1.65 1.65 0 001-1.51V3a2 2 0 012-2 2 2 0 012 2v.09a1.65 1.65 0 001 1.51 1.65 1.65 0 001.82-.33l.06-.06a2 2 0 012.83 0 2 2 0 010 2.83l-.06.06a1.65 1.65 0 00-.33 1.82V9a1.65 1.65 0 001.51 1H21a2 2 0 012 2 2 2 0 01-2 2h-.09a1.65 1.65 0 00-1.51 1z" />',
        "color": "#80a0c4", "size": 32
    },
    "sidebar_user": {
        "svg": '<path d="M20 21v-2a4 4 0 00-4-4H8a4 4 0 00-4 4v2" /><circle cx="12" cy="7" r="4" />',
        "color": "#80a0c4", "size": 32
    },
    "sidebar_folder": {
        "svg": '<path d="M22 19a2 2 0 01-2 2H4a2 2 0 01-2-2V5a2 2 0 012-2h5l2 3h9a2 2 0 012 2z" />',
        "color": "#80a0c4", "size": 32
    },
}

DOCK_ICONS = {
    "dock_system": {
        "svg": '<circle cx="12" cy="12" r="5"/><circle cx="12" cy="12" r="9"/>',
        "color": "#00f0ff", "size": 48
    },
    "dock_update": {
        "svg": '<path d="M21.5 2v6h-6M21.34 15.57a10 10 0 11-.59-9.22l-4.57 4.57"/>',
        "color": "#ffffff", "size": 48
    },
    "dock_security": {
        "svg": '<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/><path d="M9 12l2 2 4-4"/>',
        "color": "#ffffff", "size": 48
    },
    "dock_monitor": {
        "svg": '<path d="M22 12h-4l-3 9L9 3l-3 9H2"/>',
        "color": "#ffffff", "size": 48
    },
    "dock_optimization": {
        "svg": '<circle cx="12" cy="12" r="10"/><path d="M12 8v4l3 3"/>',
        "color": "#ffffff", "size": 48
    },
    "dock_cleaning": {
        "svg": '<path d="M19 4L5 18M5 4l14 14M21 21H3"/>',
        "color": "#ffffff", "size": 48
    },
    "dock_power": {
        "svg": '<rect x="5" y="2" width="14" height="20" rx="2" ry="2"/><line x1="12" y1="18" x2="12.01" y2="18"/>',
        "color": "#ffffff", "size": 48
    },
    "dock_personalize": {
        "svg": '<path d="M12 2.69l5.66 4.2c.2.15.34.37.34.62v7.98c0 .25-.14.47-.34.62L12 20.31l-5.66-4.2a.75.75 0 01-.34-.62V7.51c0-.25.14-.47.34-.62L12 2.69z"/><path d="M12 2v20"/>',
        "color": "#ffffff", "size": 48
    },
    "dock_apps": {
        "svg": '<rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/>',
        "color": "#ffffff", "size": 48
    },
}

RIGHT_ICONS = {
    "right_target": {
        "svg": '<circle cx="12" cy="12" r="10"/><circle cx="12" cy="12" r="3"/><line x1="12" y1="1" x2="12" y2="5"/><line x1="12" y1="19" x2="12" y2="23"/><line x1="1" y1="12" x2="5" y2="12"/><line x1="19" y1="12" x2="23" y2="12"/>',
        "color": "#ffffff", "size": 32
    },
    "right_priority": {
        "svg": '<rect x="2" y="7" width="20" height="14" rx="2" ry="2"/><path d="M16 21V5a2 2 0 00-2-2h-4a2 2 0 00-2 2v16"/>',
        "color": "#ffffff", "size": 32
    },
    "right_explorer": {
        "svg": '<path d="M22 19a2 2 0 01-2 2H4a2 2 0 01-2-2V5a2 2 0 012-2h5l2 3h9a2 2 0 012 2z"/>',
        "color": "#ffffff", "size": 32
    },
    "right_analytics": {
        "svg": '<line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/>',
        "color": "#ffffff", "size": 32
    },
    "right_terminal": {
        "svg": '<polyline points="4 17 10 11 4 5"/><line x1="12" y1="19" x2="20" y2="19"/>',
        "color": "#ffffff", "size": 32
    },
}

TOPBAR_ICONS = {
    "top_search": {
        "svg": '<path stroke-linecap="round" stroke-linejoin="round" d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"></path>',
        "color": "#80a0c4", "size": 18
    },
    "top_user": {
        "svg": '<path stroke-linecap="round" stroke-linejoin="round" d="M16 7a4 4 0 11-8 0 4 4 0 018 0zM12 14a7 7 0 00-7 7h14a7 7 0 00-7-7z"></path>',
        "color": "#80a0c4", "size": 18
    },
    "top_bell": {
        "svg": '<path stroke-linecap="round" stroke-linejoin="round" d="M15 17h5l-1.405-1.405A2.032 2.032 0 0118 14.158V11a6.002 6.002 0 00-4-5.659V5a2 2 0 10-4 0v.341C7.67 6.165 6 8.388 6 11v3.159c0 .538-.214 1.055-.595 1.436L4 17h5m6 0v1a3 3 0 11-6 0v-1m6 0H9"></path>',
        "color": "#80a0c4", "size": 18
    },
    "top_gear": {
        "svg": '<path stroke-linecap="round" stroke-linejoin="round" d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z"></path><path stroke-linecap="round" stroke-linejoin="round" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z"></path>',
        "color": "#80a0c4", "size": 18
    },
    "top_power": {
        "svg": '<path stroke-linecap="round" stroke-linejoin="round" d="M17 16l4-4m0 0l-4-4m4 4H7m6 4v1a3 3 0 01-3 3H6a3 3 0 01-3-3V7a3 3 0 013-3h4a3 3 0 013 3v1"></path>',
        "color": "#80a0c4", "size": 18
    },
}

# ══════════════════════════════════════════════════════════════
# Rendering Engine
# ══════════════════════════════════════════════════════════════

def create_icon_html(svg_content, color, size, bg_transparent=True):
    """Create a minimal HTML page that renders a single SVG icon."""
    bg = "transparent" if bg_transparent else "#070b12"
    return f"""<!DOCTYPE html>
<html><head><style>
    body {{ margin:0; padding:0; background:{bg}; display:flex;
           align-items:center; justify-content:center;
           width:{size}px; height:{size}px; overflow:hidden; }}
    svg {{ width:{int(size*0.6)}px; height:{int(size*0.6)}px; }}
</style></head><body>
<svg viewBox="0 0 24 24" fill="none" stroke="{color}"
     stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
    {svg_content}
</svg>
</body></html>"""


def render_icons(all_icons, driver, tmp_dir):
    """Render all icons using headless Chrome and return pixel data."""
    results = {}
    total = len(all_icons)

    for idx, (name, info) in enumerate(all_icons.items()):
        size = info["size"]
        color = info["color"]
        svg = info["svg"]

        html = create_icon_html(svg, color, size)
        html_path = os.path.join(tmp_dir, f"{name}.html")
        with open(html_path, "w", encoding="utf-8") as f:
            f.write(html)

        driver.set_window_size(size, size)
        driver.get(f"file:///{html_path.replace(os.sep, '/')}")
        time.sleep(0.15)

        png_path = os.path.join(tmp_dir, f"{name}.png")
        driver.save_screenshot(png_path)

        img = Image.open(png_path).convert("RGBA")
        # Crop to exact size (Chrome may add padding)
        img = img.crop((0, 0, size, size))
        results[name] = {"img": img, "size": size}
        print(f"  [{idx+1}/{total}] {name} ({size}x{size}) OK")

    return results


def write_c_files(results, h_path, c_path):
    """Write header and source files from rendered icon data."""
    with open(h_path, "w") as hf, open(c_path, "w") as cf:
        hf.write("/*\n * Zarqaa OS - Auto-generated bitmap icons\n")
        hf.write(" * Generated by scripts/svg_to_c.py\n */\n\n")
        hf.write("#ifndef GUI_BITMAP_ICONS_H\n#define GUI_BITMAP_ICONS_H\n\n")
        hf.write("#include \"zarqaa/types.h\"\n\n")

        cf.write("#include \"gui/bitmap_icons.h\"\n\n")

        for name, data in results.items():
            img = data["img"]
            size = data["size"]
            w, h = img.size
            pixels = img.load()
            name_upper = name.upper()

            hf.write(f"extern const uint32_t bmp_{name}[{w*h}];\n")
            hf.write(f"#define BMP_{name_upper}_WIDTH {w}\n")
            hf.write(f"#define BMP_{name_upper}_HEIGHT {h}\n\n")

            cf.write(f"const uint32_t bmp_{name}[{w*h}] = {{\n    ")
            count = 0
            for y in range(h):
                for x in range(w):
                    r, g, b, a = pixels[x, y]
                    val = (a << 24) | (r << 16) | (g << 8) | b
                    cf.write(f"0x{val:08X},")
                    count += 1
                    if count % 12 == 0:
                        cf.write("\n    ")
            cf.write("\n};\n\n")

        hf.write("#endif /* GUI_BITMAP_ICONS_H */\n")

    print(f"\n  Header: {h_path}")
    print(f"  Source: {c_path}")


def main():
    print("=" * 52)
    print("  Zarqaa OS - Smart SVG-to-C Asset Pipeline v2.0  ")
    print("=" * 52 + "\n")

    # Merge all icon groups
    all_icons = {}
    all_icons.update(SIDEBAR_ICONS)
    all_icons.update(DOCK_ICONS)
    all_icons.update(RIGHT_ICONS)
    all_icons.update(TOPBAR_ICONS)

    print(f"Total icons to generate: {len(all_icons)}\n")

    # Setup Chrome
    print("[1/3] Launching headless Chrome...")
    options = Options()
    options.add_argument("--headless")
    options.add_argument("--hide-scrollbars")
    options.add_argument("--allow-file-access-from-files")
    options.add_argument("--force-device-scale-factor=2")  # 2x for crisp rendering
    driver = webdriver.Chrome(options=options)

    # Render
    tmp_dir = tempfile.mkdtemp(prefix="zarqaa_icons_")
    print(f"[2/3] Rendering {len(all_icons)} icons...\n")
    results = render_icons(all_icons, driver, tmp_dir)
    driver.quit()

    # Write C files
    print(f"\n[3/3] Writing C arrays...")
    os.makedirs(os.path.join("include", "gui"), exist_ok=True)
    os.makedirs("gui", exist_ok=True)

    write_c_files(
        results,
        os.path.join("include", "gui", "bitmap_icons.h"),
        os.path.join("gui", "bitmap_icons.c")
    )

    print(f"\nDone! {len(all_icons)} icons generated successfully.\n")


if __name__ == "__main__":
    main()
