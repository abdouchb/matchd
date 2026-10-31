#include "CacheManager.h"

extern "C" {
    #include "zarqaa/string.h"
    void* kmalloc(size_t size);
    void kfree(void* ptr);
    void kprintf(const char* fmt, ...);
}

CacheManager::CacheManager() : m_entryCount(0) {
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        m_entries[i].url[0] = '\0';
        m_entries[i].data = nullptr;
        m_entries[i].len = 0;
    }
}

CacheManager* CacheManager::getInstance() {
    static CacheManager instance;
    return &instance;
}

void CacheManager::put(const char* url, const char* data, int len) {
    if (!url || !data || len <= 0) return;

    // Check if URL is already in cache, if so, overwrite it
    for (int i = 0; i < m_entryCount; i++) {
        if (strcmp(m_entries[i].url, url) == 0) {
            kprintf("[CacheManager] Overwriting cache for %s\n", url);
            if (m_entries[i].data) {
                kfree(m_entries[i].data);
            }
            m_entries[i].data = (char*)kmalloc(len + 1);
            memcpy(m_entries[i].data, data, len);
            m_entries[i].data[len] = '\0';
            m_entries[i].len = len;
            return;
        }
    }

    // If cache is full, evict the first entry (FIFO for simplicity)
    if (m_entryCount >= MAX_CACHE_ENTRIES) {
        kprintf("[CacheManager] Cache full. Evicting %s\n", m_entries[0].url);
        if (m_entries[0].data) {
            kfree(m_entries[0].data);
        }
        
        // Shift remaining entries
        for (int i = 1; i < MAX_CACHE_ENTRIES; i++) {
            m_entries[i-1] = m_entries[i];
        }
        m_entryCount--;
    }

    kprintf("[CacheManager] Caching %s (%d bytes)\n", url, len);
    int idx = m_entryCount++;
    strncpy(m_entries[idx].url, url, sizeof(m_entries[idx].url) - 1);
    m_entries[idx].url[sizeof(m_entries[idx].url) - 1] = '\0';
    m_entries[idx].data = (char*)kmalloc(len + 1);
    memcpy(m_entries[idx].data, data, len);
    m_entries[idx].data[len] = '\0';
    m_entries[idx].len = len;
}

const char* CacheManager::get(const char* url, int& out_len) {
    if (!url) return nullptr;

    for (int i = 0; i < m_entryCount; i++) {
        if (strcmp(m_entries[i].url, url) == 0) {
            kprintf("[CacheManager] Cache hit for %s\n", url);
            out_len = m_entries[i].len;
            return m_entries[i].data;
        }
    }

    kprintf("[CacheManager] Cache miss for %s\n", url);
    return nullptr;
}

void CacheManager::clear() {
    for (int i = 0; i < m_entryCount; i++) {
        if (m_entries[i].data) {
            kfree(m_entries[i].data);
            m_entries[i].data = nullptr;
        }
        m_entries[i].url[0] = '\0';
        m_entries[i].len = 0;
    }
    m_entryCount = 0;
}
