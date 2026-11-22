/* Prepared screen/material pixels must exactly match PNG decoding and blends. */
#include "app.h"
#include "image_decode.h"
#include "image_input.h"
#include "theme_assets.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static App app;
static uint16_t fast[2][256 * 192], slow[2][256 * 192], scene[256 * 192];
static uint8_t shades[256 * 192 / 2];
static unsigned backgrounds, overlays, materials, end_seeks;
int __real_fseek(FILE *, long, int);
int __wrap_fseek(FILE *file, long offset, int origin)
{
    if (origin == SEEK_END)
        end_seeks++;
    return __real_fseek(file, offset, origin);
}
bool __real_image_decode_background(uint16_t *, const unsigned char *, size_t, uint16_t, unsigned);
bool __real_image_decode_surface(uint16_t *, const unsigned char *, size_t, unsigned);
bool __real_image_decode_material(uint8_t *, const unsigned char *, size_t);
bool __wrap_image_decode_background(uint16_t *p, const unsigned char *d, size_t n, uint16_t m,
                                    unsigned s)
{
    backgrounds++;
    return __real_image_decode_background(p, d, n, m, s);
}
bool __wrap_image_decode_surface(uint16_t *p, const unsigned char *d, size_t n, unsigned a)
{
    overlays++;
    return __real_image_decode_surface(p, d, n, a);
}
bool __wrap_image_decode_material(uint8_t *p, const unsigned char *d, size_t n)
{
    materials++;
    return __real_image_decode_material(p, d, n);
}
static void capture(ThemePackage *theme, uint16_t output[2][256 * 192])
{
    for (int screen = 0; screen < 2; screen++)
        themes_background_draw(theme, screen, output[screen]);
}
static void check_file_bounds(const char *directory)
{
    FILE *file = tmpfile();
    assert(file);
    const size_t sizes[] = {0, 23, 24, 65536, 65537, 256 * 1024 + 1, 1024 * 1024 + 1};
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        assert(!ftruncate(fileno(file), sizes[i]));
        assert(!fseek(file, 7, SEEK_SET));
        size_t bytes = 123;
        bool valid = image_file_size(file, 24, 65536, &bytes);
        assert(valid == (sizes[i] >= 24 && sizes[i] <= 65536));
        assert(bytes == (valid ? sizes[i] : 0));
        assert(ftell(file) == 7); /* Checking size must preserve an already-read header. */
    }
    size_t bytes = 123;
    assert(!image_file_size(file, 2, 1, &bytes) && !bytes);
    assert(!image_file_size(file, 0, 1, NULL));
    assert(!image_file_size(NULL, 0, 1, &bytes) && !bytes);
    assert(!fclose(file));
    file = fopen(directory, "rb");
    assert(file);
    bytes = 123;
    assert(!image_file_size(file, 0, (size_t)-1, &bytes) && !bytes);
    assert(!fclose(file));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    check_file_bounds(argv[1]);
    app_init(&app, argv[1], true, true);
    themes_open(&app);
    themes_move(&app, 1);
    themes_preview(&app);
    assert(app.themes.ready);
    ThemePackage *theme = &app.themes.candidate;
    assert(!backgrounds && !overlays && !materials);
    capture(theme, fast);
    themes_background_scene_draw(theme, scene);
    const uint16_t *palette;
    const uint8_t *field = themes_component_material(theme, THEME_ROW_SELECTED, &palette);
    assert(field && palette);
    memcpy(shades, field, sizeof shades);
    char prepared[PATH_CAP];
    snprintf(prepared, sizeof prepared, "%s", theme->prepared_assets);
    theme->prepared_assets[0] = 0;
    end_seeks = 0;
    themes_background_load(theme, true);
    themes_components_load(theme, true);
    capture(theme, slow);
    assert(backgrounds == 2 && overlays == 2 && materials == 2);
    assert(!memcmp(fast, slow, sizeof fast));
    static uint16_t png_scene[256 * 192];
    themes_background_scene_draw(theme, png_scene);
    assert(!memcmp(scene, png_scene, sizeof scene));
    field = themes_component_material(theme, THEME_ROW_SELECTED, &palette);
    assert(field && !memcmp(shades, field, sizeof shades));
    snprintf(theme->prepared_assets, sizeof theme->prepared_assets, "%s", prepared);
    theme_assets_begin(theme);
    uint16_t *illustration = NULL;
    bool wallpaper;
    assert(theme_assets_background(theme, 0, fast[0], &illustration, &wallpaper));
    assert(illustration && wallpaper);
    free(illustration);
    assert(theme_assets_material(theme, 0, shades));
    /* Personal blend changes invalidate only the affected screen. */
    theme->screen_opacity[0] = 11;
    backgrounds = overlays = 0;
    themes_background_load(theme, true);
    assert(backgrounds == 1 && overlays == 1);
    capture(theme, fast);
    theme_assets_end();
    theme->prepared_assets[0] = 0;
    themes_background_load(theme, true);
    capture(theme, slow);
    assert(!memcmp(fast, slow, sizeof fast));
    /* A corrupt prepared bundle retains the PNG route, without partial banks. */
    char *slash = strrchr(prepared, '/');
    assert(slash);
    const char *bad[] = {"broken.gfx", "invalid-flags.gfx", "invalid-length.gfx",
                         "duplicate-screen.gfx", "oversized-v2.gfx"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        strcpy(slash + 1, bad[i]);
        snprintf(theme->prepared_assets, sizeof theme->prepared_assets, "%s", prepared);
        theme_assets_begin(theme);
        assert(!theme_assets_material(theme, 0, shades));
        theme_assets_end();
    }
    assert(end_seeks == 0); /* Image bounds come from metadata, never EOF traversal. */
    themes_cancel(&app);
    puts("PASS: prepared screens/scenes/materials match PNG pixels; six decodes become zero; "
         "personal opacity overrides and corrupt bundles retain PNG fallback");
}
