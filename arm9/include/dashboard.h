#ifndef FRONTEND_DASHBOARD_H
#define FRONTEND_DASHBOARD_H
#include "types.h"
bool dashboard_relink(App *app, const char *old_path, const char *new_path);
void dashboard_load(App *app);
void dashboard_open(App *app);
void dashboard_move(App *app, int direction);
void dashboard_edit(App *app, int direction);
void dashboard_toggle(App *app);
void dashboard_limit(App *app, int direction);
bool dashboard_record_request(App *app, const char *path);
bool dashboard_clear_recent(App *app);
void dashboard_render(const App *app, uint16_t *top, uint16_t *bottom);
void dashboard_touch(App *app, int x, int y);
#endif
