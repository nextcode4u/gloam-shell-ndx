#include "../arm9/source/ui/drawing.h"
#include "paths.h"
#include "theme_assets.h"
#include "theme_motion.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static App app;
static uint16_t pixels[1024];
static uint8_t coverage[256];
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} screen;
int main(int argc, char **argv)
{
    assert(argc == 2);
    ThemePackage *theme = &app.themes.active;
    assert(path_join(theme->prepared_assets, argv[1], "good.gfx"));
    strcpy(theme->component_directory, argv[1]);
    theme->component_coverage = true;
    theme_assets_begin(theme);
    assert(theme_assets_copy(theme, false, 0, pixels, NULL) && pixels[0] == 0xfc00);
    assert(theme_assets_copy(theme, true, THEME_ROW_SELECTED, pixels, coverage));
    assert(pixels[0] == 0x801f && coverage[0] == 0x55);
    assert(theme_assets_copy(theme, true, THEME_ROW_SELECTED, pixels, NULL) && pixels[0] == 0);
    assert(!theme_assets_copy(theme, true, THEME_COMPONENT_COUNT, pixels, coverage));
    themes_components_load(theme, true);
    theme_assets_end();
    assert(themes_component(theme, THEME_ROW_SELECTED)[0] == 0x801f);
    screen.before = screen.after = 0xc0ffee;
    assert(draw_theme_sprite(theme, screen.pixels, THEME_ROW_SELECTED, -8, 185, 24, 24));
    assert(!draw_theme_sprite(theme, screen.pixels, THEME_ROW_SELECTED, 0, 0, 1024, 1));
    assert(screen.before == 0xc0ffee && screen.after == 0xc0ffee);
    themes_components_cancel();
    const char *bad[] = {"truncated.gfx", "duplicate.gfx", "oversized.gfx", "missing.gfx"};
    for (unsigned i = 0; i < 4; i++) {
        assert(path_join(theme->prepared_assets, argv[1], bad[i]));
        theme_assets_begin(theme);
        assert(!theme_assets_copy(theme, false, 0, pixels, NULL));
    }
    theme_assets_end();
    assert(path_join(theme->ambient_path, argv[1], "good.gtm"));
    const uint8_t *alpha;
    app.storage_ready = true;
    app.frame_count = 89;
    app.selection_idle_frames = 30;
    theme->ambient_ticks = 5;
    themes_motion_tick(&app);
    assert(!themes_motion_frame(&app, &alpha));
    app.frame_count = 90;
    app.selection_idle_frames = 29;
    themes_motion_tick(&app);
    assert(!themes_motion_frame(&app, &alpha));
    app.selection_idle_frames = 30;
    themes_motion_tick(&app);
    assert(themes_motion_frame(&app, &alpha)[0] == 0x801f && alpha[0] == 255);
    for (int i = 0; i < 4; i++)
        themes_motion_tick(&app);
    assert(themes_motion_frame(&app, &alpha)[0] == 0x83e0);
    app.reduced_motion = true;
    assert(themes_motion_frame(&app, &alpha)[0] == 0x801f);
    theme->focus_feedback = true;
    memset(screen.pixels, 0, sizeof screen.pixels);
    draw_theme_focus(&app, screen.pixels, 5, 5, 100, 18, 0);
    assert(screen.pixels[8 * 256 + 7] == 0);
    app.reduced_motion = false;
    theme->palette.accent = 0xffff;
    draw_theme_focus(&app, screen.pixels, 5, 5, 100, 18, 0);
    assert(screen.pixels[8 * 256 + 7] != 0);
    for (unsigned i = 0; i < 2; i++) {
        assert(path_join(theme->ambient_path, argv[1], i ? "oversized.gtm" : "truncated.gtm"));
        themes_motion_tick(&app);
        assert(!themes_motion_frame(&app, &alpha));
    }
    themes_motion_reset();
    puts("PASS: prepared sprite bounds/checksum/duplicate rejection, fallback, deferred accent, "
         "Reduced motion and clipped controls");
}
