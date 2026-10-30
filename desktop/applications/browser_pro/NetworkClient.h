#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <stddef.h>

extern "C" {
    #include "resource/resource_manager.h"
    #include "net/url.h"
    void kprintf(const char* fmt, ...);
}

class BrowserPro;

class NetworkClient {
private:
    BrowserPro* m_browser;

    static void onResourceLoaded(const char* data, int len, void* arg);

public:
    NetworkClient(BrowserPro* browser);
    ~NetworkClient();

    void fetchUrl(const char* url);
};

#endif // NETWORK_CLIENT_H
