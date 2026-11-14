#include "splash_preferences.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool splash_load_selected(Splash *splash, const App *app)
{
    splash_close(splash);
    if (!app->storage_ready || app->storage_lost || app->recovery_mode ||
        app->splash_mode == SPLASH_OFF)
        return false;
    if (app->splash_mode == SPLASH_CUSTOM && path_is_within_root(app->splash_path, app->root) &&
        splash_load(splash, app->splash_path))
        return true;
    /* A conventional filename avoids parsing/loading a whole theme at startup. */
    char path[PATH_CAP], relative[128];
    size_t length = strlen(app->theme_id);
    if (length &&
        strspn(app->theme_id, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") ==
            length) {
        snprintf(relative, sizeof relative, "themes/%s/splash.gsp", app->theme_id);
        if (path_preferences_file(path, app->root, relative) && splash_load(splash, path))
            return true;
    }
    return path_preferences_file(path, app->root, "splashes/default.gsp") &&
           splash_load(splash, path);
}

bool splash_thumbnail(Artwork *artwork, const char *path)
{
    artwork->ready = false;
    Splash splash = {0};
    if (!splash_load(&splash, path))
        return false;
    uint16_t *screens = malloc(2 * 256 * 192 * sizeof *screens);
    if (screens) {
        splash_render(&splash, 0, true, screens, screens + 256 * 192);
        artwork->width = 64;
        artwork->height = 96;
        for (int screen = 0; screen < 2; screen++)
            for (int y = 0; y < 48; y++)
                for (int x = 0; x < 64; x++)
                    artwork->pixels[(screen * 48 + y) * 64 + x] =
                        screens[screen * 256 * 192 + y * 4 * 256 + x * 4];
        artwork->ready = true;
    }
    free(screens);
    splash_close(&splash);
    return artwork->ready;
}
