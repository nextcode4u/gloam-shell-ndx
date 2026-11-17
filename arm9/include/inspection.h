#ifndef FRONTEND_INSPECTION_H
#define FRONTEND_INSPECTION_H
#include "types.h"
bool inspection_available(const App *app);
void inspection_prepare(App *app, const char *path);
void inspection_close(App *app);
void inspection_render(const App *app, uint16_t *top, uint16_t *bottom);
#endif
