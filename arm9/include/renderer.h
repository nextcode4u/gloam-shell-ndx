#ifndef FRONTEND_RENDERER_H
#define FRONTEND_RENDERER_H
#include "types.h"
/* Draw into two caller-owned 256x192 RGB555 buffers. Never accesses hardware. */
void render_app(const App *app, uint16_t *top, uint16_t *bottom, const char *clock);
#endif
