#ifndef FRONTEND_TITLE_METADATA_H
#define FRONTEND_TITLE_METADATA_H
#include "types.h"
bool title_metadata_read(const char *path, int language, char title[128]);
void title_metadata_tick(App *app);
#endif
