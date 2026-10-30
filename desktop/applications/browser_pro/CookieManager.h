#ifndef COOKIE_MANAGER_H
#define COOKIE_MANAGER_H

struct Cookie {
    char domain[128];
    char key[64];
    char value[128];
};

class CookieManager {
private:
    static const int MAX_COOKIES = 32;
    Cookie m_cookies[MAX_COOKIES];
    int m_cookieCount;

    CookieManager();

public:
    static CookieManager* getInstance();

    void setCookie(const char* domain, const char* key, const char* value);
    const char* getCookie(const char* domain, const char* key);
    
    // Helper to parse "Set-Cookie" style strings
    void parseAndSet(const char* url, const char* cookie_str);
    
    // Helper to get all cookies for a domain in "key1=val1; key2=val2" format
    int getCookiesHeader(const char* url, char* out_buf, int max_len);
};

#endif // COOKIE_MANAGER_H
