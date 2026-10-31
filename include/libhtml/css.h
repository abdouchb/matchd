#ifndef LIBHTML_CSS_H
#define LIBHTML_CSS_H

#include "libhtml/dom.h"

/* Parses an inline style string (e.g., "color: red; width: 100px;") and updates the style struct */
void css_parse_style(css_style_t *style, const char *style_str);

#endif /* LIBHTML_CSS_H */
