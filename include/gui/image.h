#ifndef GUI_IMAGE_H
#define GUI_IMAGE_H

#include "zarqaa/types.h"

// Loads an image from the given absolute path in VFS (e.g. "/hd0/icon.png")
// Returns a dynamically allocated array of 32-bit BGRA pixels.
// The caller is responsible for calling kfree() on the returned pointer.
// out_w and out_h are populated with the image dimensions.
uint32_t *gfx_load_image(const char *path, int *out_w, int *out_h);

#endif
