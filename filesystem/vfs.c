/*
 * Zarqaa OS 0.1 - Virtual File System
 * Creator: Abdelhak Chabbi
 */

#include "filesystem/vfs.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

static struct vfs_node *vfs_root = NULL;

static int strcmp_nocase(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    char c1 = *s1;
    char c2 = *s2;
    if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
    if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
    return c1 - c2;
}

static void vfs_populate_dir(struct vfs_node *dir) {
    if (dir && dir->type == VFS_DIRECTORY && dir->fat_cluster != 0 && dir->data == NULL) {
        dir->data = (void*)1; // Mark as parsed
        extern void fat_parse_dir(uint32_t start_cluster, struct vfs_node *parent);
        fat_parse_dir(dir->fat_cluster, dir);
    }
}

void vfs_init(void) {
    vfs_root = kmalloc(sizeof(struct vfs_node));
    strncpy(vfs_root->name, "/", VFS_MAX_NAME);
    vfs_root->type = VFS_DIRECTORY;
    vfs_root->size = 0;
    vfs_root->data = NULL;
    vfs_root->parent = NULL;
    vfs_root->children = NULL;
    vfs_root->next = NULL;
}

struct vfs_node *vfs_get_root(void) {
    return vfs_root;
}

struct vfs_node *vfs_resolve_path(const char *path) {
    if (!path || !vfs_root) return NULL;
    
    // Strip file:// prefix if passed from browser / file open
    if (strncmp(path, "file://", 7) == 0) {
        path += 7;
    }

    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) return vfs_root;

    struct vfs_node *current = vfs_root;
    char path_copy[VFS_MAX_PATH];
    strncpy(path_copy, path, VFS_MAX_PATH - 1);
    path_copy[VFS_MAX_PATH - 1] = '\0';
    
    char *p = path_copy;
    while (*p) {
        // Skip leading / consecutive slashes
        while (*p == '/') p++;
        if (*p == '\0') break;

        char *token = p;
        while (*p && *p != '/') p++;
        if (*p == '/') {
            *p = '\0';
            p++;
        }

        if (strcmp(token, ".") == 0) {
            continue;
        } else if (strcmp(token, "..") == 0) {
            if (current->parent) current = current->parent;
            continue;
        }

        vfs_populate_dir(current);
        
        // Find child
        struct vfs_node *child = current->children;
        while (child) {
            if (strcmp_nocase(child->name, token) == 0) break;
            child = child->next;
        }
        if (!child) return NULL; // Not found
        
        current = child;
    }

    return current;
}

struct vfs_node *vfs_open(const char *path) {
    return vfs_resolve_path(path);
}

extern int fat_read_file(struct vfs_node *node, void *buffer, size_t size, size_t offset);

int vfs_read(struct vfs_node *node, void *buffer, size_t size, size_t offset) {
    if (!node || node->type != VFS_FILE || !buffer) return -1;
    
    if (offset >= node->size) return 0;
    
    // If it has a FAT cluster, use the FAT driver
    if (node->fat_cluster != 0) {
        return fat_read_file(node, buffer, size, offset);
    }
    
    // Otherwise it's an in-memory ramfs file
    if (!node->data) return -1;
    
    size_t to_read = size;
    if (offset + size > node->size) {
        to_read = node->size - offset;
    }
    
    memcpy(buffer, (uint8_t *)node->data + offset, to_read);
    return to_read;
}

int vfs_write(struct vfs_node *node, const void *data, size_t size) {
    if (!node || node->type != VFS_FILE || !data) return -1;
    
    // We only support overwrite/append by replacing entirely for this simple OS
    if (node->data) {
        kfree(node->data);
    }
    
    node->data = kmalloc(size);
    if (!node->data) return -1;
    
    memcpy(node->data, data, size);
    node->size = size;
    
    return size;
}

int vfs_readdir(struct vfs_node *dir, struct vfs_dirent *entries, int max_entries) {
    if (!dir || dir->type != VFS_DIRECTORY || !entries) return -1;
    
    vfs_populate_dir(dir);
    
    int count = 0;
    struct vfs_node *child = dir->children;
    
    while (child && count < max_entries) {
        strncpy(entries[count].name, child->name, VFS_MAX_NAME);
        entries[count].type = child->type;
        entries[count].size = child->size;
        
        count++;
        child = child->next;
    }
    
    return count;
}

struct vfs_node *vfs_create_dir(struct vfs_node *parent, const char *name) {
    if (!parent || parent->type != VFS_DIRECTORY) return NULL;
    
    struct vfs_node *node = kmalloc(sizeof(struct vfs_node));
    strncpy(node->name, name, VFS_MAX_NAME);
    node->type = VFS_DIRECTORY;
    node->size = 0;
    node->data = NULL;
    node->fat_cluster = 0;
    node->parent = parent;
    node->children = NULL;
    node->next = parent->children;
    parent->children = node;
    
    return node;
}

struct vfs_node *vfs_create_file(struct vfs_node *parent, const char *name, size_t size) {
    if (!parent || parent->type != VFS_DIRECTORY) return NULL;
    
    struct vfs_node *node = kmalloc(sizeof(struct vfs_node));
    strncpy(node->name, name, VFS_MAX_NAME);
    node->type = VFS_FILE;
    node->size = size;
    node->data = NULL;
    node->fat_cluster = 0;
    node->parent = parent;
    node->children = NULL;
    node->next = parent->children;
    parent->children = node;
    
    return node;
}

struct vfs_node *vfs_create_fat_node(struct vfs_node *parent, const char *name, int type, size_t size, uint32_t cluster) {
    if (!parent || parent->type != VFS_DIRECTORY) return NULL;
    
    struct vfs_node *node = kmalloc(sizeof(struct vfs_node));
    strncpy(node->name, name, VFS_MAX_NAME);
    node->type = type;
    node->size = size;
    node->data = NULL;
    node->fat_cluster = cluster;
    node->parent = parent;
    node->children = NULL;
    node->next = parent->children;
    parent->children = node;
    
    return node;
}

struct vfs_node *vfs_create(const char *path, int type) {
    if (!path) return NULL;
    
    // Split into parent path and new name
    char parent_path[VFS_MAX_PATH];
    char name[VFS_MAX_NAME];
    
    const char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        strncpy(parent_path, "/", VFS_MAX_PATH);
        strncpy(name, path, VFS_MAX_NAME);
    } else {
        if (last_slash == path) {
            strncpy(parent_path, "/", VFS_MAX_PATH);
        } else {
            int len = last_slash - path;
            strncpy(parent_path, path, len);
            parent_path[len] = '\0';
        }
        strncpy(name, last_slash + 1, VFS_MAX_NAME);
    }
    
    struct vfs_node *parent = vfs_resolve_path(parent_path);
    if (!parent || parent->type != VFS_DIRECTORY) return NULL;
    
    // Check if already exists
    struct vfs_node *child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) return NULL; // Exists
        child = child->next;
    }
    
    struct vfs_node *new_node = kmalloc(sizeof(struct vfs_node));
    if (!new_node) return NULL;
    
    strncpy(new_node->name, name, VFS_MAX_NAME);
    new_node->type = type;
    new_node->size = 0;
    new_node->data = NULL;
    new_node->parent = parent;
    new_node->children = NULL;
    
    new_node->next = parent->children;
    parent->children = new_node;
    
    return new_node;
}

int vfs_delete(const char *path) {
    if (!path) return -1;
    if (strcmp(path, "/") == 0) return -1; // Can't delete root
    
    struct vfs_node *node = vfs_resolve_path(path);
    if (!node) return -1;
    
    if (node->children) return -1; // Can't delete non-empty directory
    
    struct vfs_node *parent = node->parent;
    if (!parent) return -1;
    
    if (parent->children == node) {
        parent->children = node->next;
    } else {
        struct vfs_node *curr = parent->children;
        while (curr && curr->next != node) {
            curr = curr->next;
        }
        if (curr) {
            curr->next = node->next;
        }
    }
    
    if (node->data) kfree(node->data);
    kfree(node);
    return 0;
}
