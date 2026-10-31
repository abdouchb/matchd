#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s <font.ttf> <pixel_height> <output.h>\n", argv[0]);
        return 1;
    }

    float font_size = atof(argv[2]);
    
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        printf("Failed to open %s\n", argv[1]);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    unsigned char *ttf_buffer = malloc(size);
    fread(ttf_buffer, 1, size, f);
    fclose(f);
    
    /* Allocate bitmap 512x512 for a basic atlas */
    unsigned char *temp_bitmap = malloc(512 * 512);
    stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 chars
    
    stbtt_BakeFontBitmap(ttf_buffer, 0, font_size, temp_bitmap, 512, 512, 32, 96, cdata);
    
    FILE *out = fopen(argv[3], "w");
    if (!out) {
        printf("Failed to open %s\n", argv[3]);
        return 1;
    }
    
    fprintf(out, "/* Auto-generated Font Atlas */\n");
    fprintf(out, "#ifndef FONT_ATLAS_H\n#define FONT_ATLAS_H\n\n");
    fprintf(out, "#include \"zarqaa/types.h\"\n\n");
    
    fprintf(out, "#define FONT_ATLAS_WIDTH 512\n");
    fprintf(out, "#define FONT_ATLAS_HEIGHT 512\n");
    fprintf(out, "#define FONT_SIZE %d\n\n", (int)font_size);
    
    fprintf(out, "typedef struct {\n");
    fprintf(out, "    uint16_t x0, y0, x1, y1;\n");
    fprintf(out, "    int xoff, yoff, xadvance;\n");
    fprintf(out, "} baked_char_t;\n\n");
    
    fprintf(out, "static const baked_char_t font_cdata[96] = {\n");
    for (int i = 0; i < 96; i++) {
        fprintf(out, "    {%d, %d, %d, %d, %d, %d, %d},\n",
                cdata[i].x0, cdata[i].y0, cdata[i].x1, cdata[i].y1,
                (int)cdata[i].xoff, (int)cdata[i].yoff, (int)cdata[i].xadvance);
    }
    fprintf(out, "};\n\n");
    
    fprintf(out, "static const uint8_t font_bitmap[512*512] = {\n    ");
    for (int i = 0; i < 512 * 512; i++) {
        fprintf(out, "0x%02X, ", temp_bitmap[i]);
        if ((i + 1) % 16 == 0) fprintf(out, "\n    ");
    }
    fprintf(out, "\n};\n\n");
    fprintf(out, "#endif\n");
    
    fclose(out);
    free(ttf_buffer);
    free(temp_bitmap);
    
    printf("Font baked successfully!\n");
    return 0;
}
