#include "splash_preferences.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool load_candidate(Splash *splash, const char *path, bool reduced_motion)
{
    if (splash_load(splash, path) && splash_prepare(splash, 0, reduced_motion))
        return true;
    splash_close(splash);
    return false;
}

bool splash_load_selected(Splash *splash, const App *app)
{
    splash_close(splash);
    if (!app->storage_ready || app->storage_lost || app->recovery_mode ||
        app->splash_mode == SPLASH_OFF)
        return false;
    if (app->splash_mode == SPLASH_CUSTOM && path_is_within_root(app->splash_path, app->root) &&
        load_candidate(splash, app->splash_path, app->reduced_motion))
        return true;
    /* A conventional filename avoids parsing/loading a whole theme at startup. */
    char path[PATH_CAP], relative[128];
    size_t length = strlen(app->theme_id);
    if (length &&
        strspn(app->theme_id, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") ==
            length) {
        snprintf(relative, sizeof relative, "themes/%s/splash.gsp", app->theme_id);
        if (path_preferences_file(path, app->root, relative) &&
            load_candidate(splash, path, app->reduced_motion))
            return true;
    }
    if (path_preferences_file(path, app->root, "themes/default/splash.gsp") &&
        load_candidate(splash, path, app->reduced_motion))
        return true;
    /* Retain the original fallback for installations with a personal default. */
    return path_preferences_file(path, app->root, "splashes/default.gsp") &&
           load_candidate(splash, path, app->reduced_motion);
}

bool splash_thumbnail(Artwork *artwork, const char *path)
{
    artwork->ready = false;
    Splash splash = {0};
    if (!splash_load(&splash, path))
        return false;
    uint16_t *screens = malloc(2 * 256 * 192 * sizeof *screens);
    if (screens && splash_prepare(&splash, 0, true)) {
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
