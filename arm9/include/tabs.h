#ifndef FRONTEND_TABS_H
#define FRONTEND_TABS_H
#include "types.h"

/* Built-in indices remain stable; custom folder tabs follow configuration order. */
void tabs_load(App *app);
int tabs_find(const App *app, const char *id);
const char *tabs_label(const App *app, int index);
/* Navigation boundary: libraries/custom tabs use their assigned folder; Files uses SD. */
const char *tabs_browse_root(const App *app, int index);
void tabs_ensure_visible(App *app);
#endif
