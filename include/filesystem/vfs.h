/*
 * Zarqaa OS 0.1 - Virtual File System Header
 * Creator: Abdelhak Chabbi
 */

#ifndef FILESYSTEM_VFS_H
#define FILESYSTEM_VFS_H

#include "zarqaa/types.h"

#define VFS_MAX_PATH 256
#define VFS_MAX_NAME 64

#define VFS_FILE 1
#define VFS_DIRECTORY 2

struct vfs_node {
    char name[VFS_MAX_NAME];
    int type;
    size_t size;
    void *data;
    uint32_t fat_cluster; /* Used for FAT block devices */
    struct vfs_node *parent;
    struct vfs_node *children;
    struct vfs_node *next;
};

struct vfs_dirent {
    char name[VFS_MAX_NAME];
    int type;
    uint64_t size;
};

void vfs_init(void);
struct vfs_node *vfs_get_root(void);
struct vfs_node *vfs_resolve_path(const char *path);
struct vfs_node *vfs_open(const char *path);
int vfs_read(struct vfs_node *node, void *buffer, size_t size, size_t offset);
int vfs_write(struct vfs_node *node, const void *data, size_t size);
struct vfs_node *vfs_create_file(struct vfs_node *parent, const char *name, size_t size);
struct vfs_node *vfs_create_dir(struct vfs_node *parent, const char *name);
struct vfs_node *vfs_create_fat_node(struct vfs_node *parent, const char *name, int type, size_t size, uint32_t cluster);
int vfs_delete(const char *path);
int vfs_readdir(struct vfs_node *dir, struct vfs_dirent *entries, int max_entries);
struct vfs_node *vfs_create(const char *path, int type);

#endif /* FILESYSTEM_VFS_H */
