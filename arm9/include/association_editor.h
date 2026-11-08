#ifndef FRONTEND_ASSOCIATION_EDITOR_H
#define FRONTEND_ASSOCIATION_EDITOR_H
#include "types.h"
void association_editor_open(App *app);
void association_editor_move(App *app, int direction);
void association_editor_back(App *app);
void association_editor_activate(App *app);
void association_editor_touch(App *app, int x, int y);
void association_editor_render(const App *app, uint16_t *top, uint16_t *bottom);
#endif
