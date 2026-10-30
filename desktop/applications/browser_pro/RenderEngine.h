#ifndef RENDER_ENGINE_H
#define RENDER_ENGINE_H

#include <stddef.h>

extern "C" {
    // Lexbor HTML & CSS includes
    #include "lexbor/html/html.h"
    #include "lexbor/css/css.h"
    #include "lexbor/dom/dom.h"
}

class LayoutBox;

// C++ Wrapper for Lexbor DOM and CSS Rendering
class RenderEngine {
private:
    lxb_html_parser_t* m_html_parser;
    lxb_html_document_t* m_document;
    
    // Lexbor CSS Engine components
    lxb_css_parser_t* m_css_parser;
    lxb_css_memory_t* m_css_memory;
    lxb_css_stylesheet_t* m_stylesheet;
    
    LayoutBox* m_layoutRoot;

public:
    RenderEngine();
    ~RenderEngine();

    bool init();
    bool parseHTML(const char* html_data, size_t length);
    void computeStyles();
    void fetchSubresources(const char* base_url);
    void layout(int viewport_width, int viewport_height);
    void paint(void* surface);
    
    // Cleanup the current DOM
    void clear();
};

#endif // RENDER_ENGINE_H
