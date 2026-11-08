#ifndef FRONTEND_STORAGE_STATE_H
#define FRONTEND_STORAGE_STATE_H
#include "types.h"
/* Removal latches until restart: cached FAT state must not be reused. */
void storage_observe(App *app, bool present);
void storage_render(const App *app, uint16_t *top, uint16_t *bottom);
#endif
