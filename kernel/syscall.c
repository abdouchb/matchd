#include "kernel/syscall.h"
#include "kernel/scheduler.h"
#include "zarqaa/types.h"
#include "zarqaa/string.h"
#include "drivers/usb/usb_hid.h"
#include "filesystem/vfs.h"

static bool is_valid_user_ptr(const void *ptr, size_t len) {
    uint64_t addr = (uint64_t)ptr;
    if (addr >= 0x0000800000000000) return false;
    if (addr + len < addr) return false;
    if (addr + len > 0x0000800000000000) return false;
    return true;
}

static bool is_valid_user_string(const char *str) {
    if (!str) return false;
    return (uint64_t)str < 0x800000000000;
}

typedef void (*stdout_hook_t)(const char *str, size_t len);
static stdout_hook_t g_stdout_hook = NULL;

void terminal_register_stdout_hook(stdout_hook_t hook) {
    g_stdout_hook = hook;
}

void syscall_dispatcher(struct interrupt_frame *frame) {
    uint64_t syscall_num = frame->rax;
    
    switch(syscall_num) {
        case SYS_EXIT:
            kprintf("[SYSCALL] Task Exiting...\n");
            task_exit();
            break;
            
        case SYS_OPEN: {
            const char *path = (const char *)frame->rdi;
            if (!is_valid_user_string(path)) {
                frame->rax = -1;
                break;
            }
            int flags = frame->rsi; // Now implemented!
            struct vfs_node *node = vfs_open(path);
            if (!node) {
                frame->rax = -1;
            } else {
                int fd = scheduler_allocate_fd(node);
                if (fd >= 0) {
                    struct file_descriptor *fdesc = scheduler_get_fd(fd);
                    fdesc->flags = flags;
                }
                frame->rax = fd;
            }
            break;
        }

        case SYS_READ: {
            int fd = frame->rdi;
            void *buf = (void *)frame->rsi;
            size_t len = frame->rdx;
            
            if (!is_valid_user_ptr(buf, len)) {
                frame->rax = -1;
                break;
            }
            
            if (fd == 0) { // stdin

                char *cbuf = (char *)buf;
                size_t read_count = 0;
                for (size_t i = 0; i < len; i++) {
                    cbuf[i] = keyboard_getchar();
                    read_count++;
                    if (cbuf[i] == '\n') break;
                }
                frame->rax = read_count;
                break;
            }
            
            struct file_descriptor *fdesc = scheduler_get_fd(fd);
            if (!fdesc) {
                frame->rax = -1;
            } else {
                int read_bytes = vfs_read(fdesc->node, buf, len, fdesc->offset);
                if (read_bytes > 0) {
                    fdesc->offset += read_bytes;
                }
                frame->rax = read_bytes;
            }
            break;
        }

        case SYS_WRITE: {
            int fd = frame->rdi;
            const void *buf = (const void *)frame->rsi;
            size_t len = frame->rdx;
            
            if (!is_valid_user_ptr(buf, len)) {
                frame->rax = -1;
                break;
            }
            
            if (fd == 1 || fd == 2) { // stdout/stderr
                char temp[256];
                if (len < 256) {
                    strncpy(temp, (const char *)buf, len);
                    temp[len] = '\0';
                    kprintf("%s", temp);
                    if (g_stdout_hook) {
                        g_stdout_hook(temp, len);
                    }
                }
                frame->rax = len;
            } else {
                struct file_descriptor *fdesc = scheduler_get_fd(fd);
                if (!fdesc) {
                    frame->rax = -1;
                } else {
                    // VFS write doesn't take offset yet in ZarqaaOS, but we should advance it conceptually
                    int written = vfs_write(fdesc->node, buf, len);
                    if (written > 0) {
                        fdesc->offset += written;
                    }
                    frame->rax = written;
                }
            }
            break;
        }
        
        case SYS_CLOSE: {
            int fd = frame->rdi;
            struct file_descriptor *fdesc = scheduler_get_fd(fd);
            if (fdesc) {
                // Conceptually we'd call vfs_close(fdesc->node) here
                scheduler_free_fd(fd);
                frame->rax = 0;
            } else {
                frame->rax = -1;
            }
            break;
        }
        
        case SYS_YIELD:
            task_yield();
            frame->rax = 0;
            break;
            
        case SYS_MMAP: {
            uint64_t req_addr = frame->rdi;
            size_t length = frame->rsi;
            int prot = frame->rdx;
            
            extern uint64_t scheduler_mmap(uint64_t req_addr, size_t length, int prot);
            uint64_t mapped_addr = scheduler_mmap(req_addr, length, prot);
            if (mapped_addr == 0) {
                frame->rax = -1;
            } else {
                frame->rax = mapped_addr;
            }
            break;
        }
            
        case SYS_EXEC: {
            const char *path = (const char *)frame->rdi;
            const char *name = (const char *)frame->rsi;
            if (!is_valid_user_string(path)) {
                frame->rax = -1;
                break;
            }
            frame->rax = process_exec(path, name ? name : "app");
            break;
        }

        case 12: { // SYS_POLLKEY
            char c = 0;
            if (keyboard_poll(&c)) {
                frame->rax = (uint8_t)c;
            } else {
                frame->rax = 0;
            }
            break;
        }
        
        case 13: { // SYS_SLEEP
            uint64_t ms = frame->rdi;
            extern uint64_t pit_get_ticks(void);
            uint64_t start_ticks = pit_get_ticks();
            uint64_t needed_ticks = (ms * 100 + 999) / 1000;
            if (needed_ticks == 0) needed_ticks = 1;
            
            while (pit_get_ticks() - start_ticks < needed_ticks) {
                task_yield();
            }
            frame->rax = 0;
            break;
        }
        
        default:
            kprintf("[SYSCALL] Unimplemented Syscall: %d\n", syscall_num);
            frame->rax = -1;
            break;
    }
}
