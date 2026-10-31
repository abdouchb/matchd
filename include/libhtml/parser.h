#ifndef LIBHTML_PARSER_H
#define LIBHTML_PARSER_H

#include "libhtml/dom.h"

/* Parses an HTML string and returns the root DOM node */
dom_node_t *html_parse(const char *html);

#endif /* LIBHTML_PARSER_H */
