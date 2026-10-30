/*
 * Zarqaa OS 0.1 - Early Boot Screen Logger
 * Creator: Abdelhak Chabbi
 *
 * Displays real-time boot progress directly on the framebuffer
 * BEFORE the desktop or GUI subsystems are initialized.
 * Safe to call immediately after fb_init().
 */

#ifndef KERNEL_EARLY_BOOT_LOG_H
#define KERNEL_EARLY_BOOT_LOG_H

#include "zarqaa/types.h"

/* Log levels */
#define BLOG_INFO    0   /* White  - normal message    */
#define BLOG_OK      1   /* Green  - success           */
#define BLOG_WARN    2   /* Yellow - warning           */
#define BLOG_FAIL    3   /* Red    - error / failure   */
#define BLOG_STEP    4   /* Cyan   - step header       */

/* Initialize the early boot logger (call after fb_init) */
void early_blog_init(void);

/* Print a log line with a level badge, time, and module */
void early_blog(int level, const char *module, const char *msg);

/* Print a "[ OK ]" or "[FAIL]" result next to the last message */
void early_blog_ok(void);
void early_blog_fail(const char *reason);

/* Print a horizontal separator */
void early_blog_sep(void);

/* Force the current buffer to the screen */
void early_blog_flush(void);

/* Disable early boot logger once desktop GUI initializes */
void early_blog_finish(void);

#endif /* KERNEL_EARLY_BOOT_LOG_H */
