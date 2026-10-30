#include "BrowserTab.h"
#include "BrowserPro.h"
#include "NetworkClient.h"
#include "CacheManager.h"
#include "CookieManager.h"

extern "C" {
    #include "zarqaa/string.h"
    #include "desktop/window_manager.h"
    void kprintf(const char* fmt, ...);
}

BrowserTab::BrowserTab() : m_historyIndex(-1), m_historyCount(0), m_isLoading(false) {
    m_currentUrl[0] = '\0';
    m_tabTitle[0] = '\0';
    m_renderEngine = new RenderEngine();
}

BrowserTab::~BrowserTab() {
    delete m_renderEngine;
}

bool BrowserTab::init() {
    return m_renderEngine->init();
}

void BrowserTab::loadUrl(const char* url, bool pushToHistory) {
    if (!url) return;
    m_isLoading = true;

    kprintf("[BrowserTab] Loading URL: %s\n", url);
    strncpy(m_currentUrl, url, sizeof(m_currentUrl) - 1);
    m_currentUrl[sizeof(m_currentUrl) - 1] = '\0';

    // Simple tab title based on URL host
    const char* host_start = strstr(url, "://");
    if (host_start) {
        host_start += 3;
    } else {
        host_start = url;
    }
    int len = 0;
    while (host_start[len] && host_start[len] != '/' && len < 15) {
        m_tabTitle[len] = host_start[len];
        len++;
    }
    m_tabTitle[len] = '\0';
    if (len == 0) {
        strcpy(m_tabTitle, "New Tab");
    }

    if (pushToHistory) {
        // Clear history forward if we were back
        if (m_historyIndex < m_historyCount - 1) {
            m_historyCount = m_historyIndex + 1;
        }

        if (m_historyCount < MAX_HISTORY) {
            m_historyIndex++;
            strncpy(m_history[m_historyIndex], url, sizeof(m_history[m_historyIndex]) - 1);
            m_history[m_historyIndex][sizeof(m_history[m_historyIndex]) - 1] = '\0';
            m_historyCount = m_historyIndex + 1;
        } else {
            // Shift history
            for (int i = 1; i < MAX_HISTORY; i++) {
                strcpy(m_history[i-1], m_history[i]);
            }
            m_historyIndex = MAX_HISTORY - 1;
            strncpy(m_history[m_historyIndex], url, sizeof(m_history[m_historyIndex]) - 1);
            m_history[m_historyIndex][sizeof(m_history[m_historyIndex]) - 1] = '\0';
        }
    }

    // Templates removed by user request. Proceed directly to Cache / Network.

    // Check Cache first
    int cached_len = 0;
    const char* cached_data = CacheManager::getInstance()->get(url, cached_len);
    if (cached_data) {
        kprintf("[BrowserTab] Serving %s from CacheManager\n", url);
        m_isLoading = false;
        m_renderEngine->parseHTML(cached_data, cached_len);
        m_renderEngine->computeStyles();
        m_renderEngine->fetchSubresources(m_currentUrl);
        
        // Force redraw of browser window
        struct window* win = BrowserPro::getInstance()->getWindow();
        if (win) {
            m_renderEngine->layout(win->width, win->height - 110); // Y offset of 110px
            win->needs_redraw = true;
        }
        return;
    }

    // Load from Network client
    BrowserPro::getInstance()->loadUrl(url); // Route network fetch via BrowserPro
}

void BrowserTab::onResourceLoaded(const char* data, int len) {
    m_isLoading = false;
    if (!data || len < 0) {
        kprintf("[BrowserTab] Resource load failed!\n");
        return;
    }

    // Save to Cache
    CacheManager::getInstance()->put(m_currentUrl, data, len);

    // Render it
    m_renderEngine->parseHTML(data, len);
    m_renderEngine->computeStyles();
    m_renderEngine->fetchSubresources(m_currentUrl);
    
    // Force redraw of browser window
    struct window* win = BrowserPro::getInstance()->getWindow();
    if (win) {
        m_renderEngine->layout(win->width, win->height - 110); // Y offset of 110px
        win->needs_redraw = true;
    }
}

bool BrowserTab::goBack() {
    if (canGoBack()) {
        m_historyIndex--;
        loadUrl(m_history[m_historyIndex], false);
        return true;
    }
    return false;
}

bool BrowserTab::goForward() {
    if (canGoForward()) {
        m_historyIndex++;
        loadUrl(m_history[m_historyIndex], false);
        return true;
    }
    return false;
}

bool BrowserTab::canGoBack() const {
    return m_historyIndex > 0;
}

bool BrowserTab::canGoForward() const {
    return m_historyIndex < m_historyCount - 1;
}
