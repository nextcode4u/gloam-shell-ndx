#ifndef GLOAM_LOCALIZATION_H
#define GLOAM_LOCALIZATION_H
#include "types.h"
/* Catalog discovery is bounded and runs on opening settings, never during drawing. */
void localization_load(App *app);
void localization_scan(App *app);
void localization_adjust(App *app, int direction);
#endif
