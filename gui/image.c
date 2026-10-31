#include "gui/graphics.h"
#include "kernel/heap.h"
#include "filesystem/vfs.h"
#include "zarqaa/string.h"
#include "zarqaa/types.h"

// Define macros to make stb_image freestanding
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG

#define STBI_MALLOC kmalloc
#define STBI_REALLOC krealloc
#define STBI_FREE kfree
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ASSERT(x) ((void)0)

// abs is provided by our compat/stdlib.h stub

// Include the header
#include "../libzui/stb_image.h"

// Function to load an image from VFS into a 32-bit RGBA pixel array
uint32_t *gfx_load_image(const char *path, int *out_w, int *out_h) {
    if (!path) return NULL;
    struct vfs_node *file = vfs_open(path);
    if (!file || file->type != VFS_FILE || file->size == 0) return NULL;
    
    // Allocate buffer for raw file
    uint8_t *file_data = kmalloc(file->size);
    if (!file_data) return NULL;
    
    // Read the entire file
    int rd = vfs_read(file, file_data, file->size, 0);
    if (rd <= 0) {
        kfree(file_data);
        return NULL;
    }
    
    // Decode image (req: 4 channels -> RGBA)
    int width, height, channels;
    uint8_t *img_data = stbi_load_from_memory(file_data, file->size, &width, &height, &channels, 4);
    
    kfree(file_data); // Free raw file
    
    if (!img_data) return NULL; // Failed to decode
    
    // Convert to 32-bit pixels (BGRA format for our framebuffer)
    uint32_t *pixels = kmalloc(width * height * 4);
    if (!pixels) {
        STBI_FREE(img_data);
        return NULL;
    }

    for (int i = 0; i < width * height; i++) {
        uint8_t r = img_data[i * 4 + 0];
        uint8_t g = img_data[i * 4 + 1];
        uint8_t b = img_data[i * 4 + 2];
        uint8_t a = img_data[i * 4 + 3];
        
        pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    
    STBI_FREE(img_data); // Free stb buffer
    
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
    
    return pixels;
}
