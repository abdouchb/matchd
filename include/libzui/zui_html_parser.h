/*
 * Zarqaa UI (ZUI) Mini HTML Parser
 * Creator: Abdelhak Chabbi
 */

#ifndef LIBZUI_ZUI_HTML_PARSER_H
#define LIBZUI_ZUI_HTML_PARSER_H

#include "libzui/zui.h"

/*
 * Parses a simple HTML string and appends generated ZUI widgets to parent_panel.
 * Supported tags for Proof of Concept:
 * - <h1>...</h1> -> ZLabel (large, colored)
 * - <p>...</p> -> ZLabel (normal)
 * - <button>...</button> -> ZButton
 */
void zui_parse_html(zui_widget_t *parent_panel, const char *html);

#endif /* LIBZUI_ZUI_HTML_PARSER_H */
