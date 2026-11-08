#ifndef FRONTEND_APPEARANCE_H
#define FRONTEND_APPEARANCE_H
#include "types.h"
#define APPEARANCE_PRESET_COUNT 3
const char *appearance_preset_name(int slot);
bool appearance_save(App *app, int slot);
bool appearance_load(App *app, int slot);
void appearance_reset(App *app);
#endif
