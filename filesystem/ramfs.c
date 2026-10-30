/*
 * Zarqaa OS 0.1 - RAM Filesystem
 * Creator: Abdelhak Chabbi
 */

#include "filesystem/ramfs.h"
#include "filesystem/vfs.h"
#include "zarqaa/string.h"

void ramfs_init(void) {
    vfs_create("/system", VFS_DIRECTORY);
    vfs_create("/apps", VFS_DIRECTORY);
    vfs_create("/users", VFS_DIRECTORY);
    vfs_create("/documents", VFS_DIRECTORY);

    struct vfs_node *version_txt = vfs_create("/system/version.txt", VFS_FILE);
    if (version_txt) {
        const char *content = "Zarqaa OS 0.1\nCreator: Abdelhak Chabbi\n";
        vfs_write(version_txt, content, strlen(content));
    }

    struct vfs_node *readme_txt = vfs_create("/documents/readme.txt", VFS_FILE);
    if (readme_txt) {
        const char *content = "Welcome to Zarqaa OS!\nThis is a real x86_64 operating system.\nEnjoy exploring the UI and filesystem.\n";
        vfs_write(readme_txt, content, strlen(content));
    }

    struct vfs_node *notes_txt = vfs_create("/documents/notes.txt", VFS_FILE);
    if (notes_txt) {
        const char *content = "Your notes go here.\n";
        vfs_write(notes_txt, content, strlen(content));
    }
}
