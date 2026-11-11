#ifndef FRONTEND_GRID_H
#define FRONTEND_GRID_H
#include "types.h"
void browser_jump_initial(App *app, int direction);
void browser_direction(App *app, int horizontal, int vertical);
int grid_touch_index(const App *app, int x, int y);
#endif
