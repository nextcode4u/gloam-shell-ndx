#ifndef FRONTEND_ASSOCIATIONS_H
#define FRONTEND_ASSOCIATIONS_H
#include "types.h"
void associations_load(App *app);
bool association_extension_valid(const char *extension);
const Association *association_user(const App *app, const char *extension);
bool association_set_default(App *app, const char *extension, const char *application);
bool association_disable(App *app, const char *extension);
bool association_clear_default(App *app, const char *extension);
const char *association_cover(const App *app, const char *path);
const Association *association_find(const App *app, const char *path);
/* Resolves display metadata only. It does not launch applications. */
bool association_icon_source(const App *app, const Entry *entry, char output[PATH_CAP]);
bool association_icon_application(const App *app, const char *path, char output[PATH_CAP]);
#endif
