#pragma once

#include "graphics/libvector/path.h"

// Parses an SVG path data string (the 'd' attribute) and populates a path_t object
// Returns true on success, false on failure
bool svg_parse_path_data(const char *d_attr, path_t *out_path);
