/* Two active and two preview surfaces. All decoding happens in controllers. */
#include "app.h"
#include "image_decode.h"
#include "image_input.h"
#include "image_job.h"
#include "paths.h"
#include "theme_assets.h"
#include "themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t *pixels, *scene;
    char path[PATH_CAP], surface_path[PATH_CAP];
    uint16_t matte;
    unsigned strength, content_opacity;
    bool ready, wallpaper_ready;
} Background;
static Background surfaces[2][2];

static bool decode_file(Background *background, bool surface)
{
    const char *path = surface ? background->surface_path : background->path;
    if (image_job_cancelled() || !*path || (!surface && !background->strength))
        return false;
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    size_t bytes;
    size_t limit = surface ? 256 * 1024 : 1024 * 1024;
    bool valid = image_file_size(file, 1, limit, &bytes);
    unsigned char *encoded = valid ? malloc(bytes) : NULL;
    valid = encoded && fread(encoded, 1, bytes, file) == bytes;
    fclose(file);
    app_loading_checkpoint();
    if (valid && !image_job_cancelled()) {
        unsigned opacity = background->content_opacity <= 100 ? background->content_opacity : 100;
        valid = surface ? image_decode_surface(background->pixels, encoded, (size_t)bytes, opacity)
                        : image_decode_background(background->pixels, encoded, (size_t)bytes,
                                                  background->matte, background->strength);
    }
    free(encoded);
    return valid && !image_job_cancelled();
}

static bool load_surface(Background *background, const ThemePackage *package, int screen)
{
    free(background->pixels);
    free(background->scene);
    memset(background, 0, sizeof *background);
    background->matte = package->palette.bg;
    background->strength = package->background_strength;
    background->content_opacity = themes_content_opacity(package, screen);
    path_copy(background->path, package->backgrounds[screen], PATH_CAP);
    path_copy(background->surface_path, package->reading_surfaces[screen], PATH_CAP);
    if ((!*background->path || !background->strength) && !*background->surface_path)
        return false;
    background->pixels = malloc(256 * 192 * sizeof *background->pixels);
    if (!background->pixels)
        return false;
    if (theme_assets_background(package, screen, background->pixels, &background->scene,
                                &background->wallpaper_ready)) {
        background->ready = true;
        return true;
    }
    for (int i = 0; i < 256 * 192; i++)
        background->pixels[i] = background->matte;
    background->wallpaper_ready = decode_file(background, false);
    background->ready = background->wallpaper_ready;
    /* Preserve only the exposed 94-line illustration strip. Dialogs keep the
     * normal preblended reading surface; the browser can reveal this strip. */
    if (background->ready && screen == 0 && package->top_layout == 3) {
        background->scene = malloc(256 * 94 * sizeof *background->scene);
        if (background->scene)
            memcpy(background->scene, background->pixels + 256 * 24,
                   256 * 94 * sizeof *background->scene);
    }
    /* An authored surface replaces the flat reading mat. Missing/malformed assets
     * retain the old mat, including its independent screen opacity. No extra
     * framebuffer or alpha plane is retained after this load. */
    bool authored_surface = decode_file(background, true);
    background->ready |= authored_surface;
    if (!authored_surface && background->ready && background->content_opacity <= 100) {
        unsigned alpha = background->content_opacity;
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
    if (!background->ready) {
        free(background->pixels);
        background->pixels = NULL;
    }
    return background->ready;
}
void themes_background_load(const ThemePackage *package, bool preview)
{
    for (int screen = 0; screen < 2 && !image_job_cancelled(); screen++) {
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
    for (int bank = 1; bank >= 0; bank--) {
        const Background *background = &surfaces[bank][screen];
        if ((!bank || themes_preview_visible()) && background->ready &&
            !strcmp(background->path, package->backgrounds[screen]) &&
            !strcmp(background->surface_path, package->reading_surfaces[screen]) &&
            background->matte == package->palette.bg &&
            background->strength == package->background_strength &&
            background->content_opacity == themes_content_opacity(package, screen)) {
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
        if ((!bank || themes_preview_visible()) && background->ready && background->scene &&
            !strcmp(background->path, package->backgrounds[0]) &&
            !strcmp(background->surface_path, package->reading_surfaces[0]) &&
            background->matte == package->palette.bg &&
            background->strength == package->background_strength &&
            background->content_opacity == themes_content_opacity(package, 0)) {
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
    /* Font roles make a package too large for a temporary ARM9 stack copy. */
    ThemePackage *candidate = malloc(sizeof *candidate);
    if (!candidate)
        return false;
    *candidate = app->themes.active;
    path_copy(candidate->backgrounds[screen], path, PATH_CAP);
    if (!candidate->background_strength)
        candidate->background_strength = 25;
    bool loaded = load_surface(&surfaces[1][screen], candidate, screen) &&
                  surfaces[1][screen].wallpaper_ready;
    free(candidate);
    if (!loaded) {
        free(surfaces[1][screen].pixels);
        free(surfaces[1][screen].scene);
        memset(&surfaces[1][screen], 0, sizeof surfaces[1][screen]);
        return false;
    }
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
