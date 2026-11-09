#ifndef FRONTEND_APP_H
#define FRONTEND_APP_H
#include "types.h"

/* Navigation and actions are shared by the DSi executable and host tests. */
void app_init(App *app, const char *root, bool storage_ready, bool dsi_mode);
void app_init_mode(App *app, const char *root, bool storage_ready, bool dsi_mode, bool recovery);
void app_category(App *app, int category);
void app_move(App *app, int delta);
void app_rail_toggle(App *app);
void app_rail_move(App *app, int direction);
void app_rail_commit(App *app);
void app_toggle_details(App *app);
void app_page(App *app, int direction);
void app_open(App *app);
void app_back(App *app);
void app_request_system_menu(App *app);
void app_panel(App *app, Panel panel);
void app_panel_close(App *app);
void app_panel_move(App *app, int delta);
void app_panel_activate(App *app);
void app_panel_adjust(App *app, int direction);
bool app_adjust_control(App *app, QuickControl control, int direction);
void app_tick(App *app);
bool app_is_favorite(const App *app, const char *path);
void app_toggle_favorite(App *app);
void app_mark_preferences_dirty(App *app);
void app_reset_preview(App *app);
#endif
