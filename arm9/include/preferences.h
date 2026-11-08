#ifndef FRONTEND_PREFERENCES_H
#define FRONTEND_PREFERENCES_H
#include "types.h"

/* Read the current state, its backup, or the original scratch's legacy state. */
void preferences_load(App *app);
/* Save only our state; false means that a file operation failed. */
bool app_save(App *app);
#endif
