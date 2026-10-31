#ifndef BROWSER_PRO_H
#define BROWSER_PRO_H

#include <stddef.h>
#include <stdint.h>

extern "C" {
    struct window;
    typedef struct zui_context zui_context_t;
    void kprintf(const char* fmt, ...);
    void* kmalloc(size_t size);
    void kfree(void* ptr);
}

class BrowserTab;
class NetworkClient;

class BrowserPro {
private:
    static const int MAX_TABS = 8;
    struct window* m_window;
    NetworkClient* m_networkClient;
    
    BrowserTab* m_tabs[MAX_TABS];
    int m_activeTab;
    int m_tabCount;

    // URL bar editing state
    char m_urlInputBuffer[256];
    bool m_urlBarFocused;

public:
    BrowserPro();
    ~BrowserPro();

    bool init();
    void loadUrl(const char* url); // Main trigger
    void onResourceLoaded(const char* data, int len);
    
    void paint(void* canvas);
    
    // UI Event Handlers
    void handleMouse(int mx, int my, bool left, bool right);
    void handleKey(char key, uint8_t scancode);

    struct window* getWindow() const { return m_window; }
    void setWindow(struct window* w) { m_window = w; }

    BrowserTab* getActiveTab() const {
        if (m_activeTab >= 0 && m_activeTab < m_tabCount) {
            return m_tabs[m_activeTab];
        }
        return nullptr;
    }

    static BrowserPro* getInstance();
};

#endif // BROWSER_PRO_H
