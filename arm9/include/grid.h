#ifndef FRONTEND_GRID_H
#define FRONTEND_GRID_H
#include "types.h"
int browser_index_group(const App *app, int index);
void browser_index_rebuild(App *app);
void browser_index_jump(App *app, int direction);
void browser_jump_initial(App *app, int direction);
void browser_direction(App *app, int horizontal, int vertical);
int grid_touch_index(const App *app, int x, int y);
#endif
