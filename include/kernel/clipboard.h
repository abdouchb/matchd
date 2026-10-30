#ifndef KERNEL_CLIPBOARD_H
#define KERNEL_CLIPBOARD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void clipboard_init(void);
void clipboard_set_text(const char *text);
const char *clipboard_get_text(void);

#endif
