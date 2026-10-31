#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include <stddef.h>

struct CacheEntry {
    char url[256];
    char* data;
    int len;
};

class CacheManager {
private:
    static const int MAX_CACHE_ENTRIES = 16;
    CacheEntry m_entries[MAX_CACHE_ENTRIES];
    int m_entryCount;

    CacheManager();

public:
    static CacheManager* getInstance();

    void put(const char* url, const char* data, int len);
    const char* get(const char* url, int& out_len);
    void clear();
};

#endif // CACHE_MANAGER_H
