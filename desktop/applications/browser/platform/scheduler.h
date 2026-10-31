#ifndef BROWSER_SCHEDULER_H
#define BROWSER_SCHEDULER_H

#include "zarqaa/types.h"

void browser_scheduler_init(void);
void browser_scheduler_invalidate_layout(void);
void browser_scheduler_invalidate_paint(void);
bool browser_scheduler_needs_layout(void);
bool browser_scheduler_needs_paint(void);
void browser_scheduler_clear_flags(void);
void browser_scheduler_clear_layout_flag(void);
void browser_scheduler_clear_paint_flag(void);

#endif
