#ifndef GUI_NEW_APP_ICONS_H
#define GUI_NEW_APP_ICONS_H

#include "zarqaa/types.h"

enum { NEWICON_CLOCK, NEWICON_CALENDAR, NEWICON_SYSINFO, NEWICON_2048, NEWICON_MINES, NEWICON_COUNT };

extern uint32_t new_icon_dock[NEWICON_COUNT][1600];
extern uint32_t new_icon_menu[NEWICON_COUNT][576];

void new_app_icons_init(void);

#endif
