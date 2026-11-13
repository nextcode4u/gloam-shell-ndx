#ifndef FRONTEND_PREFERENCES_H
#define FRONTEND_PREFERENCES_H
#include "types.h"

/* Read the current state, its backup, or the original scratch's legacy state. */
void preferences_load(App *app);
/* Save only our state. On failure, preferences_error identifies the operation
 * and filesystem error; previous settings/recovery copies are preserved. */
bool app_save(App *app);
#endif
