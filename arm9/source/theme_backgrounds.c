/* Two active and two preview surfaces. All decoding happens in controllers. */
#include "app.h"
#include "image_decode.h"
#include "paths.h"
#include "themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t *pixels, *scene;
    char path[PATH_CAP];
    uint16_t matte;
    unsigned strength, content_opacity;
    bool ready;
} Background;
static Background surfaces[2][2];

static bool load_surface(Background *background, const ThemePackage *package, int screen)
{
    free(background->pixels);
    free(background->scene);
    memset(background, 0, sizeof *background);
    background->matte = package->palette.bg;
    background->strength = package->background_strength;
    background->content_opacity = package->content_opacity;
    path_copy(background->path, package->backgrounds[screen], PATH_CAP);
    if (!*background->path || !background->strength)
        return false;
    FILE *file = fopen(background->path, "rb");
    if (!file)
        return false;
    bool valid = !fseek(file, 0, SEEK_END);
    long bytes = valid ? ftell(file) : -1;
    unsigned char *encoded = bytes > 0 && bytes <= 1024 * 1024 ? malloc((size_t)bytes) : NULL;
    valid = encoded && !fseek(file, 0, SEEK_SET) &&
            fread(encoded, 1, (size_t)bytes, file) == (size_t)bytes;
    fclose(file);
    if (valid)
        background->pixels = malloc(256 * 192 * sizeof *background->pixels);
    background->ready = valid && background->pixels &&
                        image_decode_background(background->pixels, encoded, (size_t)bytes,
                                                background->matte, background->strength);
    /* Preserve only the exposed 94-line illustration strip. Dialogs keep the
     * normal preblended reading surface; the browser can reveal this strip. */
    if (background->ready && screen == 0 && package->top_layout == 3) {
        background->scene = malloc(256 * 94 * sizeof *background->scene);
        if (background->scene)
            memcpy(background->scene, background->pixels + 256 * 24,
                   256 * 94 * sizeof *background->scene);
    }
    /* Preblend the reading surface once, never in the frame loop. */
    if (background->ready && package->content_opacity <= 100) {
        unsigned alpha = package->content_opacity;
        for (int y = 4; alpha && y < 188; y++)
            for (int x = 6; x < 250; x++) {
                uint16_t old = background->pixels[y * 256 + x];
                unsigned color = 0x8000;
                for (unsigned shift = 0; shift < 15; shift += 5)
                    color |= ((((old >> shift) & 31) * (100 - alpha) +
                               ((background->matte >> shift) & 31) * alpha) /
                              100)
                             << shift;
                background->pixels[y * 256 + x] = color;
            }
    }
    free(encoded);
    if (!background->ready) {
        free(background->pixels);
        background->pixels = NULL;
    }
    return background->ready;
}
void themes_background_load(const ThemePackage *package, bool preview)
{
    for (int screen = 0; screen < 2; screen++) {
        load_surface(&surfaces[preview ? 1 : 0][screen], package, screen);
        app_loading_checkpoint();
    }
}
static void move_surface(Background *destination, Background *source)
{
    free(destination->pixels);
    free(destination->scene);
    *destination = *source;
    memset(source, 0, sizeof *source);
}
void themes_background_accept(void)
{
    for (int screen = 0; screen < 2; screen++)
        move_surface(&surfaces[0][screen], &surfaces[1][screen]);
}
void themes_background_cancel(void)
{
    for (int screen = 0; screen < 2; screen++) {
        free(surfaces[1][screen].pixels);
        free(surfaces[1][screen].scene);
        memset(&surfaces[1][screen], 0, sizeof surfaces[1][screen]);
    }
}
void themes_background_draw(const ThemePackage *package, int screen, uint16_t *pixels)
{
    if (screen < 0 || screen >= 2)
        return;
    for (int bank = 0; bank < 2; bank++) {
        const Background *background = &surfaces[bank][screen];
        if (background->ready && !strcmp(background->path, package->backgrounds[screen]) &&
            background->matte == package->palette.bg &&
            background->strength == package->background_strength &&
            background->content_opacity == package->content_opacity) {
            memcpy(pixels, background->pixels, 256 * 192 * sizeof *pixels);
            return;
        }
    }
    for (int i = 0; i < 256 * 192; i++)
        pixels[i] = package->palette.bg;
}
void themes_background_scene_draw(const ThemePackage *package, uint16_t *pixels)
{
    for (int bank = 1; bank >= 0; bank--) {
        const Background *background = &surfaces[bank][0];
        if (background->ready && background->scene &&
            !strcmp(background->path, package->backgrounds[0]) &&
            background->matte == package->palette.bg &&
            background->strength == package->background_strength &&
            background->content_opacity == package->content_opacity) {
            memcpy(pixels + 256 * 24, background->scene, 256 * 94 * sizeof *pixels);
            return;
        }
    }
}

bool themes_background_set(App *app, int screen, const char *path)
{
    if (app->recovery_mode || !app->storage_ready || screen < 0 || screen > 1 ||
        !path_is_within_root(path, app->root))
        return false;
    ThemePackage candidate = app->themes.active;
    path_copy(candidate.backgrounds[screen], path, PATH_CAP);
    if (!candidate.background_strength)
        candidate.background_strength = 25;
    if (!load_surface(&surfaces[1][screen], &candidate, screen))
        return false;
    move_surface(&surfaces[0][screen], &surfaces[1][screen]);
    path_copy(app->themes.active.backgrounds[screen], path, PATH_CAP);
    if (!app->themes.active.background_strength) {
        app->wallpaper_strength = 25;
        app->themes.active.background_strength = 25;
        themes_background_load(&app->themes.active, false);
    }
    path_copy(app->custom_backgrounds[screen], path, PATH_CAP);
    app_mark_preferences_dirty(app);
    return true;
}
