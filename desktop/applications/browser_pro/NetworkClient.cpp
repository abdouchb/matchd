#include "NetworkClient.h"
#include "BrowserPro.h"

NetworkClient::NetworkClient(BrowserPro* browser) : m_browser(browser) {
}

NetworkClient::~NetworkClient() {
}

void NetworkClient::onResourceLoaded(const char* data, int len, void* arg) {
    BrowserPro* browser = static_cast<BrowserPro*>(arg);
    
    if (len < 0) {
        kprintf("[NetworkClient] Failed to load resource (code %d)\n", len);
        // Error handling could be here
        return;
    }
    
    kprintf("[NetworkClient] Loaded %d bytes, passing to RenderEngine...\n", len);
    browser->onResourceLoaded(data, len);
}

void NetworkClient::fetchUrl(const char* url) {
    parsed_url_t parsed;
    if (!url_parse(url, &parsed)) {
        kprintf("[NetworkClient] Invalid URL format: %s\n", url);
        m_browser->onResourceLoaded(nullptr, -1);
        return;
    }
    
    kprintf("[NetworkClient] Fetching %s ...\n", parsed.full_url);
    // Safe: m_browser is a process-lifetime singleton (BrowserPro::getInstance())
    resource_load(&parsed, NetworkClient::onResourceLoaded, m_browser);
}
