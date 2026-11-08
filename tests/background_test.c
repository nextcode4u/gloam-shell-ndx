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
#include <string.h>
static App app, restored, recovery;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} top, bottom;
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
    assert(!image_decode_background(top.pixels, data, bytes, 0x8421, 36));
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
