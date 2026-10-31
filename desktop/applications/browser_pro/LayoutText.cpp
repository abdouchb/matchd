#include "LayoutText.h"

extern "C" {
    #include "zarqaa/string.h"
    #include "desktop/window_manager.h"
    void* kmalloc(size_t size);
    void kfree(void* ptr);
}

#include "GraphicsUtils.h"

#define CHAR_WIDTH  8
#define LINE_HEIGHT 16

struct LineSpan { int start, end; bool has_trailing_space; };

static int computeLines(const char* text, int text_len, int chars_per_line, LineSpan* out, int max_lines) {
    int num_lines = 0;
    int line_start = 0;
    while (line_start < text_len && num_lines < max_lines) {
        int line_end = line_start + chars_per_line;
        if (line_end >= text_len) {
            if (out) { out[num_lines].start = line_start; out[num_lines].end = text_len; out[num_lines].has_trailing_space = false; }
            num_lines++;
            break;
        }
        int break_at = line_end;
        bool found_space = false;
        for (int i = line_end; i > line_start; i--) {
            if (text[i] == ' ') {
                break_at = i;
                found_space = true;
                break;
            }
        }
        if (!found_space) break_at = line_end;
        if (out) {
            out[num_lines].start = line_start;
            out[num_lines].end = break_at;
            out[num_lines].has_trailing_space = found_space;
        }
        num_lines++;
        line_start = found_space ? break_at + 1 : break_at;
    }
    return num_lines;
}

LayoutText::LayoutText(struct lxb_dom_node* node, const char* text) : LayoutBox(node) {
    if (text) {
        size_t len = strlen(text);
        m_text = (char*)kmalloc(len + 1);
        if (m_text) {
            strcpy(m_text, text);
        } else {
            // Fallback: If kmalloc fails, silently fallback to empty text behavior.
            // layout() and paint() safely handle m_text == nullptr.
        }
    } else {
        m_text = nullptr;
    }
    style.display = DisplayType::Inline;
    m_num_lines = 1;
    m_max_width = 800;
}

LayoutText::~LayoutText() {
    if (m_text) {
        kfree(m_text);
    }
}

void LayoutText::layout(int max_width) {
    if (!m_text || m_text[0] == '\0') {
        width = 0;
        height = 0;
        m_num_lines = 0;
        return;
    }
    
    m_max_width = max_width > 0 ? max_width : 800;
    int text_len = strlen(m_text);
    int full_width = text_len * CHAR_WIDTH;
    
    if (full_width <= m_max_width) {
        // Fits on one line
        width = full_width;
        height = LINE_HEIGHT;
        m_num_lines = 1;
    } else {
        // Need to word-wrap
        int chars_per_line = m_max_width / CHAR_WIDTH;
        if (chars_per_line < 1) chars_per_line = 1;
        
        LineSpan spans[100]; // Assuming max 100 lines for a single text block
        m_num_lines = computeLines(m_text, text_len, chars_per_line, spans, 100);
        
        int max_line_width = 0;
        for (int i = 0; i < m_num_lines; i++) {
            int line_w = (spans[i].end - spans[i].start) * CHAR_WIDTH;
            if (spans[i].has_trailing_space) line_w += CHAR_WIDTH; // Include space for width if needed, or maybe not needed
            if (line_w > max_line_width) max_line_width = line_w;
        }
        
        width = max_line_width > 0 ? max_line_width : m_max_width;
        height = m_num_lines * LINE_HEIGHT;
    }
}

void LayoutText::paint(struct window* win) {
    if (!m_text || m_text[0] == '\0') return;
    
    int abs_x = 0, abs_y = 0;
    LayoutBox* curr = this;
    while (curr) {
        abs_x += curr->x;
        abs_y += curr->y;
        curr = curr->parent;
    }
    
    int text_len = strlen(m_text);
    
    if (m_num_lines <= 1) {
        // Single line - just draw it
        // Clip to avoid drawing off-screen
        if (abs_y >= 0 && abs_y < win->height) {
            draw_text(win, abs_x, abs_y, m_text, style.color);
        }
    } else {
        // Multi-line word-wrapped rendering
        int chars_per_line = m_max_width / CHAR_WIDTH;
        if (chars_per_line < 1) chars_per_line = 1;
        
        LineSpan spans[100];
        int actual_lines = computeLines(m_text, text_len, chars_per_line, spans, 100);
        
        int line_y = abs_y;
        char line_buf[256];
        
        for (int i = 0; i < actual_lines && line_y < win->height; i++) {
            if (line_y >= 0) {
                int copy_len = spans[i].end - spans[i].start;
                if (copy_len > 255) copy_len = 255;
                memcpy(line_buf, m_text + spans[i].start, copy_len);
                line_buf[copy_len] = '\0';
                
                draw_text(win, abs_x, line_y, line_buf, style.color);
            }
            line_y += LINE_HEIGHT;
        }
    }
}
