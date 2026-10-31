#ifndef BROWSER_TAB_H
#define BROWSER_TAB_H

#include "RenderEngine.h"

class BrowserTab {
private:
    static const int MAX_HISTORY = 16;
    char m_history[MAX_HISTORY][256];
    int m_historyIndex;
    int m_historyCount;

    char m_currentUrl[256];
    char m_tabTitle[64];
    RenderEngine* m_renderEngine;
    bool m_isLoading;

public:
    BrowserTab();
    ~BrowserTab();

    bool init();
    void loadUrl(const char* url, bool pushToHistory = true);
    
    bool goBack();
    bool goForward();
    bool canGoBack() const;
    bool canGoForward() const;

    const char* getUrl() const { return m_currentUrl; }
    const char* getTitle() const { return m_tabTitle; }
    RenderEngine* getRenderEngine() const { return m_renderEngine; }
    bool isLoading() const { return m_isLoading; }
    
    void onResourceLoaded(const char* data, int len);
};

#endif // BROWSER_TAB_H
