#include "BrowserPro.h"
#include "BrowserTab.h"
#include "NetworkClient.h"
#include "CacheManager.h"
#include "CookieManager.h"
#include "Theme.h"
#include "GraphicsUtils.h"

extern "C" {
    #include "zarqaa/string.h"
    #include "desktop/window_manager.h"
    #include "gui/graphics.h"
    #include "gui/renderer.h"
    void kprintf(const char* fmt, ...);
    struct window *wm_create_window(const char *title, int x, int y, int w, int h);
    struct window *browser_pro_open(void);
    void browser_pro_on_draw(struct window *win);
    void browser_pro_on_mouse(struct window *win, int mx, int my, bool left, bool right);
    void browser_pro_on_key(struct window *win, char key, uint8_t scancode);
}

static BrowserPro* g_instance = nullptr;

BrowserPro* BrowserPro::getInstance() {
    if (!g_instance) {
        g_instance = new BrowserPro();
    }
    return g_instance;
}

BrowserPro::BrowserPro() : m_window(nullptr), m_activeTab(0), m_tabCount(0), m_urlBarFocused(false) {
    m_networkClient = new NetworkClient(this);
    m_urlInputBuffer[0] = '\0';
    
    for (int i = 0; i < MAX_TABS; i++) {
        m_tabs[i] = nullptr;
    }
}

BrowserPro::~BrowserPro() {
    delete m_networkClient;
    for (int i = 0; i < m_tabCount; i++) {
        delete m_tabs[i];
    }
}

bool BrowserPro::init() {
    kprintf("[BrowserPro] Initializing Zarqaa Browser Pro tabs...\n");
    
    // Create first tab
    BrowserTab* first = new BrowserTab();
    if (!first->init()) {
        kprintf("[BrowserPro] Failed to init first tab!\n");
        delete first;
        return false;
    }
    m_tabs[0] = first;
    m_tabCount = 1;
    m_activeTab = 0;
    
    return true;
}

void BrowserPro::loadUrl(const char* url) {
    m_networkClient->fetchUrl(url);
}

void BrowserPro::onResourceLoaded(const char* data, int len) {
    BrowserTab* active = getActiveTab();
    if (active) {
        active->onResourceLoaded(data, len);
    }
}

void BrowserPro::paint(void* canvas) {
    struct window* win = (struct window*)canvas;
    if (!win || !win->content_buffer) return;

    // 1. Draw Browser Chrome UI background
    draw_rect(win, 0, 0, win->width, 110, QP_BG_DARK);

    // 2. Draw Tab Bar (y=0 to 35)
    for (int i = 0; i < m_tabCount; i++) {
        int tab_x = i * 140 + 10;
        uint32_t tab_color = (i == m_activeTab) ? QP_BG_PANEL : QP_MNT_DARK;
        uint32_t text_color = (i == m_activeTab) ? QP_CYAN : QP_TEXT_MUTED;
        
        // Draw tab background (rounded top)
        draw_rounded_rect(win, tab_x, 5, 130, 30, 8, tab_color);
        // Fill bottom so active tab connects smoothly to navigation bar
        if (i == m_activeTab) {
            draw_rect(win, tab_x, 25, 130, 10, tab_color);
            // Highlight line on top of active tab
            draw_rect(win, tab_x + 10, 5, 110, 2, QP_CYAN);
        } else {
            // Faint border for inactive tabs
            draw_rect(win, tab_x, 5, 130, 1, QP_BORDER);
        }
        
        // Tab title
        draw_text(win, tab_x + 12, 12, m_tabs[i]->getTitle(), text_color);

        // Close tab button (x)
        uint32_t close_bg = (i == m_activeTab) ? QP_MNT_LIGHT : QP_MNT_DARK;
        draw_rounded_rect(win, tab_x + 110, 10, 16, 16, 4, close_bg);
        draw_text(win, tab_x + 114, 12, "x", QP_TEXT_MAIN);
    }

    // New Tab (+) button
    int add_x = m_tabCount * 140 + 15;
    draw_rounded_rect(win, add_x, 10, 20, 20, 10, QP_BORDER);
    draw_text(win, add_x + 6, 12, "+", QP_CYAN);

    // 3. Draw Navigation Bar (y=35 to 80)
    draw_rect(win, 0, 35, win->width, 45, QP_BG_PANEL);
    
    BrowserTab* active = getActiveTab();
    if (active) {
        // Back Button
        uint32_t back_col = active->canGoBack() ? QP_MNT_LIGHT : QP_BG_PANEL;
        uint32_t back_text = active->canGoBack() ? QP_TEXT_MAIN : QP_TEXT_DIM;
        draw_rounded_rect(win, 10, 42, 30, 30, 15, back_col);
        draw_text(win, 20, 49, "<", back_text);

        // Forward Button
        uint32_t fwd_col = active->canGoForward() ? QP_MNT_LIGHT : QP_BG_PANEL;
        uint32_t fwd_text = active->canGoForward() ? QP_TEXT_MAIN : QP_TEXT_DIM;
        draw_rounded_rect(win, 45, 42, 30, 30, 15, fwd_col);
        draw_text(win, 55, 49, ">", fwd_text);

        // Unified Omnibox (URL Input)
        uint32_t url_box_col = m_urlBarFocused ? QP_ACTIVE_BG : QP_MNT_LIGHT;
        uint32_t text_col = m_urlBarFocused ? QP_CYAN : QP_TEXT_MAIN;
        draw_rounded_rect(win, 85, 42, win->width - 150, 30, 15, url_box_col);
        
        // Draw Lock icon inside Omnibox
        draw_rounded_rect(win, 95, 49, 12, 16, 4, QP_TEXT_DIM); // Padlock body
        draw_rect(win, 99, 46, 4, 3, QP_TEXT_DIM); // Padlock shackle
        
        // If focused, draw current buffer, otherwise draw active URL
        if (m_urlBarFocused) {
            draw_text(win, 115, 49, m_urlInputBuffer, text_col);
            // Draw cursor
            int len = 0; while(m_urlInputBuffer[len]) len++;
            draw_rect(win, 115 + len * 8, 49, 2, 14, QP_CYAN);
        } else {
            draw_text(win, 115, 49, active->getUrl(), text_col);
        }

        // Settings / Menu button
        draw_rounded_rect(win, win->width - 50, 42, 30, 30, 15, QP_BG_PANEL);
        draw_text(win, win->width - 43, 49, "...", QP_CYAN);
    }
    
    // 4. Draw Bookmarks Bar (y=80 to 110)
    draw_rect(win, 0, 80, win->width, 30, QP_BG_PANEL);
    draw_text(win, 15, 87, "Zarqaa OS", QP_TEXT_MUTED);
    draw_text(win, 100, 87, "GitHub", QP_TEXT_MUTED);
    draw_text(win, 160, 87, "C++ Reference", QP_TEXT_MUTED);
    
    // Draw separator line below UI
    draw_rect(win, 0, 109, win->width, 1, QP_CYAN);

    // 5. Fill content area with white background (standard browser default)
    draw_rect(win, 0, 110, win->width, win->height - 110, 0xFFFFFFFF);

    // 6. Paint the active tab content
    if (active && active->getRenderEngine()) {
        active->getRenderEngine()->paint(win);
    }
}

void BrowserPro::handleMouse(int mx, int my, bool left, bool right) {
    (void)right;
    if (!left) return;

    // A. Check Tab Bar clicks (y < 35)
    if (my < 35) {
        int tab_idx = mx / 140;
        if (tab_idx < m_tabCount) {
            // Check if close tab was clicked
            int tab_x = tab_idx * 140 + 10;
            if (mx >= tab_x + 110 && mx <= tab_x + 124 && my >= 10 && my <= 24) {
                kprintf("[BrowserPro] Closing tab %d\n", tab_idx);
                if (m_tabCount > 1) {
                    delete m_tabs[tab_idx];
                    for (int i = tab_idx + 1; i < m_tabCount; i++) {
                        m_tabs[i-1] = m_tabs[i];
                    }
                    m_tabCount--;
                    if (m_activeTab >= m_tabCount) {
                        m_activeTab = m_tabCount - 1;
                    }
                }
            } else {
                // Just switch tab
                m_activeTab = tab_idx;
                m_urlBarFocused = false;
                kprintf("[BrowserPro] Switched to tab %d\n", m_activeTab);
            }
            if (m_window) m_window->needs_redraw = true;
            return;
        }

        // Check if "+" button was clicked
        int add_x = m_tabCount * 140 + 15;
        if (mx >= add_x && mx <= add_x + 20 && my >= 10 && my <= 30) {
            if (m_tabCount < MAX_TABS) {
                kprintf("[BrowserPro] Creating new tab...\n");
                BrowserTab* tab = new BrowserTab();
                tab->init();
                m_tabs[m_tabCount] = tab;
                m_activeTab = m_tabCount;
                m_tabCount++;
                tab->loadUrl("www.google.com");
                if (m_window) m_window->needs_redraw = true;
            }
            return;
        }
    }

    // B. Check Navigation Bar clicks (y >= 35 && y < 80)
    if (my >= 35 && my < 80) {
        BrowserTab* active = getActiveTab();
        if (!active) return;

        // Back button
        if (mx >= 10 && mx <= 40 && my >= 42 && my <= 72) {
            active->goBack();
            if (m_window) m_window->needs_redraw = true;
            return;
        }

        // Forward button
        if (mx >= 45 && mx <= 75 && my >= 42 && my <= 72) {
            active->goForward();
            if (m_window) m_window->needs_redraw = true;
            return;
        }

        // URL input focus
        if (mx >= 85 && mx <= m_window->width - 150 + 85 && my >= 42 && my <= 72) {
            m_urlBarFocused = true;
            strncpy(m_urlInputBuffer, active->getUrl(), sizeof(m_urlInputBuffer) - 1);
            m_urlInputBuffer[sizeof(m_urlInputBuffer) - 1] = '\0';
            if (m_window) m_window->needs_redraw = true;
            return;
        }

        // Clicking anywhere else in Navigation bar defocuses URL input
        m_urlBarFocused = false;
        if (m_window) m_window->needs_redraw = true;
    }
    
    // C. Check Bookmarks Bar clicks (y >= 80 && y < 110)
    if (my >= 80 && my < 110) {
        BrowserTab* active = getActiveTab();
        if (!active) return;
        
        m_urlBarFocused = false;
        
        if (mx >= 15 && mx <= 85) {
            active->loadUrl("www.google.com");
            if (m_window) m_window->needs_redraw = true;
        } else if (mx >= 100 && mx <= 150) {
            active->loadUrl("github.com");
            if (m_window) m_window->needs_redraw = true;
        } else if (mx >= 160 && mx <= 260) {
            active->loadUrl("cppreference.com");
            if (m_window) m_window->needs_redraw = true;
        }
    }
}

void BrowserPro::handleKey(char key, uint8_t scancode) {
    if (!m_urlBarFocused) return;
    
    // Check for Enter
    if (key == '\n' || key == '\r' || scancode == 0x1C) {
        m_urlBarFocused = false;
        BrowserTab* active = getActiveTab();
        if (active) {
            active->loadUrl(m_urlInputBuffer);
        }
        if (m_window) m_window->needs_redraw = true;
        return;
    }

    // Check for Backspace
    if (key == '\b' || key == 0x08 || scancode == 0x0E) {
        int len = strlen(m_urlInputBuffer);
        if (len > 0) {
            m_urlInputBuffer[len - 1] = '\0';
            if (m_window) m_window->needs_redraw = true;
        }
        return;
    }

    // Otherwise, append regular character
    int len = strlen(m_urlInputBuffer);
    if (len < (int)sizeof(m_urlInputBuffer) - 1 && key >= 32 && key <= 126) {
        m_urlInputBuffer[len] = key;
        m_urlInputBuffer[len + 1] = '\0';
        if (m_window) m_window->needs_redraw = true;
    }
}

extern "C" void browser_pro_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)win;
    BrowserPro* pro = BrowserPro::getInstance();
    if (pro) {
        pro->handleMouse(mx, my, left, right);
    }
}

extern "C" void browser_pro_on_key(struct window *win, char key, uint8_t scancode) {
    (void)win;
    BrowserPro* pro = BrowserPro::getInstance();
    if (pro) {
        pro->handleKey(key, scancode);
    }
}

extern "C" void browser_pro_on_draw(struct window *win) {
    BrowserPro* pro = BrowserPro::getInstance();
    if (pro) {
        pro->paint(win);
    }
}

extern "C" struct window *browser_pro_open(void) {
    kprintf("[BrowserPro] Starting Tabbed Browser Pro architecture...\n");
    BrowserPro* pro = BrowserPro::getInstance();
    
    struct window* win = wm_create_window("Browser Pro", 150, 150, 800, 600);
    pro->setWindow(win);
    
    if (!pro->getWindow()) {
        kprintf("[BrowserPro] Failed to create window!\n");
        return nullptr;
    }
    
    // Hook up UI callbacks
    win->on_draw = browser_pro_on_draw;
    win->on_mouse = browser_pro_on_mouse;
    win->on_key = browser_pro_on_key;
    win->needs_redraw = true;
    
    if (pro->init()) {
        pro->getActiveTab()->loadUrl("www.google.com");
    }
    
    return pro->getWindow();
}
