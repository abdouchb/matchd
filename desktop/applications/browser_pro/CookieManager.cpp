#include "CookieManager.h"

extern "C" {
    #include "zarqaa/string.h"
    void kprintf(const char* fmt, ...);
}

// Simple helper to extract domain from URL
static void extract_domain(const char* url, char* out_domain, int max_len) {
    out_domain[0] = '\0';
    if (!url) return;

    const char* start = strstr(url, "://");
    if (start) {
        start += 3;
    } else {
        start = url;
    }

    int i = 0;
    while (start[i] && start[i] != '/' && start[i] != ':' && i < max_len - 1) {
        out_domain[i] = start[i];
        i++;
    }
    out_domain[i] = '\0';
}

CookieManager::CookieManager() : m_cookieCount(0) {
    for (int i = 0; i < MAX_COOKIES; i++) {
        m_cookies[i].domain[0] = '\0';
        m_cookies[i].key[0] = '\0';
        m_cookies[i].value[0] = '\0';
    }
}

CookieManager* CookieManager::getInstance() {
    static CookieManager instance;
    return &instance;
}

void CookieManager::setCookie(const char* domain, const char* key, const char* value) {
    if (!domain || !key || !value) return;

    // Check if cookie already exists
    for (int i = 0; i < m_cookieCount; i++) {
        if (strcmp(m_cookies[i].domain, domain) == 0 && strcmp(m_cookies[i].key, key) == 0) {
            strncpy(m_cookies[i].value, value, sizeof(m_cookies[i].value) - 1);
            m_cookies[i].value[sizeof(m_cookies[i].value) - 1] = '\0';
            kprintf("[CookieManager] Updated cookie %s=%s for %s\n", key, value, domain);
            return;
        }
    }

    // Add new cookie
    if (m_cookieCount < MAX_COOKIES) {
        int idx = m_cookieCount++;
        strncpy(m_cookies[idx].domain, domain, sizeof(m_cookies[idx].domain) - 1);
        m_cookies[idx].domain[sizeof(m_cookies[idx].domain) - 1] = '\0';
        
        strncpy(m_cookies[idx].key, key, sizeof(m_cookies[idx].key) - 1);
        m_cookies[idx].key[sizeof(m_cookies[idx].key) - 1] = '\0';

        strncpy(m_cookies[idx].value, value, sizeof(m_cookies[idx].value) - 1);
        m_cookies[idx].value[sizeof(m_cookies[idx].value) - 1] = '\0';

        kprintf("[CookieManager] Stored cookie %s=%s for %s\n", key, value, domain);
    } else {
        kprintf("[CookieManager] Cookie storage full, discarding %s=%s\n", key, value);
    }
}

const char* CookieManager::getCookie(const char* domain, const char* key) {
    if (!domain || !key) return nullptr;
    for (int i = 0; i < m_cookieCount; i++) {
        if (strcmp(m_cookies[i].domain, domain) == 0 && strcmp(m_cookies[i].key, key) == 0) {
            return m_cookies[i].value;
        }
    }
    return nullptr;
}

void CookieManager::parseAndSet(const char* url, const char* cookie_str) {
    if (!url || !cookie_str) return;

    char domain[128];
    extract_domain(url, domain, sizeof(domain));

    // Simple parse of "key=value" (ignores attributes like path, secure for simplicity)
    const char* eq = strchr(cookie_str, '=');
    if (!eq) return;

    char key[64];
    int key_len = eq - cookie_str;
    if (key_len >= (int)sizeof(key)) key_len = sizeof(key) - 1;
    memcpy(key, cookie_str, key_len);
    key[key_len] = '\0';

    // trim leading/trailing whitespace if any
    char value[128];
    const char* val_start = eq + 1;
    // read until ';' or end of string
    int i = 0;
    while (val_start[i] && val_start[i] != ';' && i < (int)sizeof(value) - 1) {
        value[i] = val_start[i];
        i++;
    }
    value[i] = '\0';

    setCookie(domain, key, value);
}

int CookieManager::getCookiesHeader(const char* url, char* out_buf, int max_len) {
    if (!url || !out_buf || max_len <= 0) return 0;
    out_buf[0] = '\0';

    char domain[128];
    extract_domain(url, domain, sizeof(domain));

    int offset = 0;
    bool first = true;
    for (int i = 0; i < m_cookieCount; i++) {
        if (strcmp(m_cookies[i].domain, domain) == 0) {
            int needed = strlen(m_cookies[i].key) + strlen(m_cookies[i].value) + 1; // key + "=" + val
            if (!first) needed += 2; // "; "
            
            if (offset + needed + 1 > max_len) break; // +1 for null terminator

            if (!first) {
                strcat(out_buf + offset, "; ");
                offset += 2;
            }
            strcat(out_buf + offset, m_cookies[i].key);
            offset += strlen(m_cookies[i].key);
            strcat(out_buf + offset, "=");
            offset += 1;
            strcat(out_buf + offset, m_cookies[i].value);
            offset += strlen(m_cookies[i].value);
            first = false;
        }
    }
    return offset;
}
