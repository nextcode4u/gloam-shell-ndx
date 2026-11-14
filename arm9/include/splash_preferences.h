#ifndef FRONTEND_SPLASH_PREFERENCES_H
#define FRONTEND_SPLASH_PREFERENCES_H
#include "splash.h"
#include "types.h"
bool splash_load_selected(Splash *splash, const App *app);
bool splash_thumbnail(Artwork *artwork, const char *path);
#endif
