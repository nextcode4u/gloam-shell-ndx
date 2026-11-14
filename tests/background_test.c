#include "app.h"
#include "image_decode.h"
#include "paths.h"
#include "picker.h"
#include "preferences.h"
#include "renderer.h"
#include "settings.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static App app, restored, recovery;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} top, bottom;

static void reading_surfaces(const char *root)
{
    char path[PATH_CAP];
    assert(path_join(path, root, "surface.png"));
    FILE *file = fopen(path, "rb");
    assert(file);
    unsigned char data[8192];
    size_t bytes = fread(data, 1, sizeof data, file);
    fclose(file);
    for (int i = 0; i < 256 * 192; i++)
        top.pixels[i] = 0xfc00;
    assert(image_decode_surface(top.pixels, data, bytes, 100));
    assert(top.pixels[32] == 0xfc00);
    assert(top.pixels[96] == (0x8000 | 10 | (21 << 10)));
    assert(top.pixels[160] == (0x8000 | 21 | (10 << 10)));
    assert(top.pixels[224] == 0x801f);
    memcpy(bottom.pixels, top.pixels, sizeof top.pixels);
    assert(!image_decode_surface(top.pixels, data, bytes - 20, 100));
    assert(!image_decode_surface(top.pixels, data, bytes, 101));
    data[19] = 1; /* Wrong width; must not crop an overlay into the wrong bounds. */
    assert(!image_decode_surface(top.pixels, data, bytes, 100));
    assert(!memcmp(top.pixels, bottom.pixels, sizeof top.pixels));

    ThemePackage *package = malloc(sizeof *package);
    assert(package);
    *package = app.themes.active;
    strcpy(package->reading_surfaces[0], path);
    package->backgrounds[0][0] = 0; /* A paper surface also works without a wallpaper. */
    package->screen_opacity[0] = 100;
    themes_background_load(package, true);
    themes_background_draw(package, 0, top.pixels);
    assert(top.pixels[32] == package->palette.bg && top.pixels[224] == 0x801f);
    /* Same wallpaper/palette must not select a preview with a different surface. */
    package->reading_surfaces[0][0] = 0;
    themes_background_draw(package, 0, bottom.pixels);
    for (int i = 0; i < 256 * 192; i++)
        assert(bottom.pixels[i] == package->palette.bg);
    strcpy(package->reading_surfaces[0], path);
    package->screen_opacity[0] = 0;
    themes_background_load(package, true);
    themes_background_draw(package, 0, top.pixels);
    for (int i = 0; i < 256 * 192; i++)
        assert(top.pixels[i] == package->palette.bg);
    package->screen_opacity[0] = 100;
    assert(path_join(package->backgrounds[0], root, "media/cover.jpg"));
    package->background_strength = 100;
    strcpy(package->reading_surfaces[0], "/nonexistent-surface.png");
    themes_background_load(package, true);
    themes_background_draw(package, 0, top.pixels);
    assert(top.pixels[32 * 256 + 32] == package->palette.bg); /* Legacy mat fallback. */
    themes_background_cancel();
    free(package);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    app_init(&app, argv[1], true, true);
    char image[PATH_CAP];
    assert(path_join(image, argv[1], "media/cover.jpg"));
    assert(themes_background_set(&app, 0, image));
    assert(!strcmp(app.custom_backgrounds[0], image));
    top.before = top.after = bottom.before = bottom.after = 0xBADC0FFE;
    themes_background_draw(&app.themes.active, 0, top.pixels);
    uint16_t background = top.pixels[0];
    assert(background != app.themes.active.palette.bg);
    themes_background_draw(&app.themes.active, 1, bottom.pixels);
    assert(bottom.pixels[0] == app.themes.active.palette.bg);
    app.themes.active.content_opacity = 100;
    app.themes.active.top_layout = 3;
    themes_background_load(&app.themes.active, false);
    themes_background_draw(&app.themes.active, 0, top.pixels);
    assert(top.pixels[0] == background);
    assert(top.pixels[4 * 256 + 6] == app.themes.active.palette.bg);
    themes_background_scene_draw(&app.themes.active, top.pixels);
    bool illustration_visible = false;
    for (int y = 24; y < 118; y++)
        for (int x = 6; x < 250; x++)
            illustration_visible |= top.pixels[y * 256 + x] != app.themes.active.palette.bg;
    assert(illustration_visible);
    assert(top.pixels[120 * 256 + 6] == app.themes.active.palette.bg);
    app.themes.active.top_layout = 0;
    app.themes.active.content_opacity = 0;
    themes_background_load(&app.themes.active, false);
    assert(!themes_background_set(&app, 1, "/outside/image.jpg"));
    assert(!themes_background_set(&app, 1, "tests/fixtures/nonexistent.jpg"));
    assert(app_save(&app));
    app_init(&restored, argv[1], true, true);
    assert(!strcmp(restored.custom_backgrounds[0], image));
    themes_background_draw(&restored.themes.active, 0, top.pixels);
    assert(top.pixels[0] == background);
    assert(picker_open_background(&restored, 1) &&
           restored.picker.kind == PICKER_BOTTOM_BACKGROUND);
    assert(restored.picker.count == 1);
    picker_activate(&restored);
    assert(!restored.picker.open && *restored.custom_backgrounds[1]);
    app_init_mode(&recovery, argv[1], true, true, true);
    themes_background_draw(&recovery.themes.active, 0, top.pixels);
    assert(top.pixels[0] == recovery.themes.active.palette.bg);
    assert(!themes_background_set(&recovery, 0, image));
    assert(!picker_open_background(&recovery, 0));
    assert(app_save(&recovery));
    app_init(&restored, argv[1], true, true);
    assert(!strcmp(restored.custom_backgrounds[0], image));
    render_app(&restored, top.pixels, bottom.pixels, "12:00");
    assert(top.before == 0xBADC0FFE && top.after == 0xBADC0FFE && bottom.before == 0xBADC0FFE &&
           bottom.after == 0xBADC0FFE);
    FILE *file = fopen(image, "rb");
    assert(file);
    unsigned char data[8192];
    size_t bytes = fread(data, 1, sizeof data, file);
    fclose(file);
    assert(image_decode_background(top.pixels, data, bytes, 0x8421, 0));
    assert(top.pixels[0] == 0x8421);
    assert(image_decode_background(top.pixels, data, bytes, 0x8421, 100));
    assert(!image_decode_background(top.pixels, data, bytes, 0x8421, 101));
    reading_surfaces(argv[1]);
    restored.advanced_settings = true;
    settings_open(&restored);
    restored.settings.selected = 10;
    restored.wallpaper_strength = 33;
    settings_adjust(&restored, 1);
    assert(restored.wallpaper_strength == 35);
    settings_adjust(&restored, 1);
    assert(restored.wallpaper_strength == -1);
    restored.wallpaper_strength = 2;
    settings_adjust(&restored, -1);
    assert(restored.wallpaper_strength == 0);
    puts("PASS: bounded PNG/JPEG wallpapers, prepared rendering, independent screen overrides, "
         "picker/persistence, strength limits and recovery bypass");
}
