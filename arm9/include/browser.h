#ifndef FRONTEND_BROWSER_H
#define FRONTEND_BROWSER_H
#include "types.h"
void app_scan(App *app);
void browser_batch(App *app, int direction);
void browser_scan_cancel(App *app);
void browser_scan_begin(App *app);
void browser_scan_step(App *app);
void browser_reflow(App *app);
void browser_refresh(App *app);
void browser_restore_selection(App *app, const char *path);
void browser_display_name(const App *app, const Entry *entry, char output[256]);
const Entry *app_selected(const App *app);
const char *entry_type(const Entry *entry);
#endif
