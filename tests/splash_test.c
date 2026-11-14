#include "app.h"
#include "paths.h"
#include "picker.h"
#include "preferences.h"
#include "settings.h"
#include "splash_preferences.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static App app, restored;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} top, bottom;
static void draw(Splash *splash, unsigned tick, bool reduced)
{
    top.before = top.after = bottom.before = bottom.after = 0x12345678;
    splash_render(splash, tick, reduced, top.pixels, bottom.pixels);
    assert(top.before == 0x12345678 && top.after == 0x12345678);
    assert(bottom.before == 0x12345678 && bottom.after == 0x12345678);
}
static void motion_checks(const char *root)
{
    char path[PATH_CAP];
    Splash splash = {0};
    assert(path_join(path, root, "motion.gsp") && splash_load(&splash, path));
    assert(splash.layers[0].motion && splash.background == 0x801F);
    draw(&splash, 11, false);
    assert(top.pixels[96 * 256 + 128] == 0x801F);
    draw(&splash, 42, false);
    assert(top.pixels[96 * 256 + 128] == 0x83E0);
    assert(top.pixels[0] == 0x801F); /* Bottom layers cannot spill to the upper display. */
    draw(&splash, 72, false);
    assert(top.pixels[96 * 256 + 128] == 0x801F);
    draw(&splash, 0, true);
    assert(top.pixels[96 * 256 + 128] == 0x83E0);
    assert(splash_needs_frame(&splash, 42, 43, false));
    assert(!splash_needs_frame(&splash, 42, 43, true));
    const unsigned scales[] = {0, 1, 25, 100, 400};
    for (unsigned reveal = 0; reveal < 5; reveal++)
        for (unsigned easing = 0; easing < 4; easing++)
            for (unsigned repeat = 0; repeat < 3; repeat++)
                for (unsigned i = 0; i < 5; i++) {
                    SplashLayer *layer = &splash.layers[0];
                    layer->effect = reveal;
                    layer->easing = easing;
                    layer->repeat = repeat;
                    layer->start_scale = scales[i];
                    layer->end_scale = scales[4 - i];
                    layer->start_x = -512;
                    layer->start_y = 512;
                    layer->end_x = 512;
                    layer->end_y = -512;
                    for (unsigned tick = 0; tick < 180; tick += 13)
                        draw(&splash, tick, false);
                }
    splash_close(&splash);
    const char *bad[] = {"scale",    "alpha",    "easing",     "repeat",   "sequence",
                         "reserved", "position", "background", "zero-time"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        char name[64];
        snprintf(name, sizeof name, "bad-motion-%s.gsp", bad[i]);
        assert(path_join(path, root, name));
        assert(!splash_load(&splash, path) && !splash.data);
    }
}

static bool option(const char *name)
{
    for (int page = 0; page < settings_section_count(&app); page++)
        for (int row = 0; row < settings_option_count(&app, page); row++)
            if (!strcmp(settings_option(&app, page, row)->label, name)) {
                app.settings.section = page;
                app.settings.selected = row;
                return true;
            }
    return false;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    motion_checks(argv[1]);
    char valid[PATH_CAP], bad[PATH_CAP];
    assert(path_join(valid, argv[1], "valid.gsp") && path_join(bad, argv[1], "bad.gsp"));
    Splash splash = {0};
    assert(splash_load(&splash, valid));
    assert(splash.duration == 120 && splash.count == 3 && !splash.sound);
    assert(splash_playing(&splash, 0, true, false, false));
    assert(!splash_playing(&splash, 0, false, false, false));
    assert(splash_playing(&splash, 119, false, true, false));
    assert(!splash_playing(&splash, 120, false, true, false));
    assert(!splash_playing(&splash, 0, true, true, true));
    assert(splash_needs_frame(&splash, UINT32_MAX, 0, false));
    assert(splash_needs_frame(&splash, 0, 30, false));
    assert(!splash_needs_frame(&splash, 30, 110, false));
    assert(!splash_needs_frame(&splash, 0, 30, true));
    draw(&splash, 0, false);
    assert(top.pixels[96 * 256 + 128] == 0x801f);
    draw(&splash, 30, false);
    assert(top.pixels[96 * 256 + 128] == 0x83e0);
    assert(bottom.pixels[96 * 256 + 128] == 0xfc00);
    draw(&splash, 0, true);
    assert(top.pixels[96 * 256 + 128] == 0x83e0);
    size_t bytes = splash.bytes;
    unsigned char *original = malloc(bytes);
    assert(original);
    memcpy(original, splash.data, bytes);
    splash_close(&splash);
    for (unsigned i = 0; i < 80; i++) {
        unsigned char saved = original[i];
        original[i] ^= 0x81;
        FILE *file = fopen(bad, "wb");
        assert(file && fwrite(original, 1, bytes, file) == bytes && !fclose(file));
        assert(!splash_load(&splash, bad) && !splash.data && !splash.sound);
        original[i] = saved;
    }
    free(original);
    const char *invalid[] = {"duration", "layers", "width", "frames", "offset", "sound"};
    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; i++) {
        char name[64];
        snprintf(name, sizeof name, "bad-%s.gsp", invalid[i]);
        assert(path_join(bad, argv[1], name));
        assert(!splash_load(&splash, bad) && !splash.data);
    }
    assert(path_join(bad, argv[1], "sound.gsp"));
    assert(splash_load(&splash, bad));
    assert(splash.sound && splash.sound_bytes == 32000);
    assert(((uintptr_t)splash.sound & 3) == 0 && splash.sound[0] == 2000);
    splash_close(&splash);
    assert(path_join(bad, argv[1], "missing.gsp"));
    assert(splash_thumbnail(&app.artwork, valid));
    assert(app.artwork.width == 64 && app.artwork.height == 96);
    app_init(&app, argv[1], true, true);
    assert(app.splash_mode == SPLASH_THEME && !app.splash_once && !app.splash_sound);
    path_copy(app.splash_path, valid, PATH_CAP);
    app.splash_mode = SPLASH_CUSTOM;
    assert(splash_load_selected(&splash, &app));
    assert(splash.layers[0].format == 1 && splash.count == 3);
    strcpy(app.theme_id, "probe");
    path_copy(app.splash_path, bad, PATH_CAP);
    assert(splash_load_selected(&splash, &app) && splash.layers[0].format == 0);
    strcpy(app.theme_id, "missing");
    assert(splash_load_selected(&splash, &app) && splash.count == 1);
    app.splash_mode = SPLASH_OFF;
    assert(!splash_load_selected(&splash, &app));
    app.splash_mode = SPLASH_CUSTOM;
    path_copy(app.splash_path, valid, PATH_CAP);
    app.theme_id[0] = 0;
    app.recovery_mode = true;
    assert(!splash_load_selected(&splash, &app) && !splash.data);
    app.recovery_mode = false;
    settings_open(&app);
    assert(option("Splash playback"));
    settings_adjust(&app, 1);
    assert(app.splash_once);
    assert(option("Splash sound"));
    settings_adjust(&app, 1);
    assert(app.splash_sound);
    assert(option("Preview splash"));
    settings_activate(&app);
    assert(app.splash_preview_requested);
    assert(app_save(&app));
    app_init(&restored, argv[1], true, true);
    assert(restored.splash_once && restored.splash_sound && restored.splash_mode == SPLASH_CUSTOM);
    assert(!strcmp(restored.splash_path, valid));
    assert(picker_open_splash(&app));
    int index = 0;
    while (index < app.picker.count && strcmp(app.picker.entries[index].name, "valid.gsp"))
        index++;
    assert(index < app.picker.count);
    picker_move(&app, index);
    for (int i = 0; i < 7; i++)
        picker_tick(&app);
    assert(app.picker.preview.ready);
    picker_activate(&app);
    assert(!app.picker.open && app.splash_mode == SPLASH_CUSTOM);
    puts("PASS: prepared splash pixels, timing, skip, reduced motion, mutation rejection, preview "
         "picker and persistence");
    return 0;
}
