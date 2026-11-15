#include "../arm9/source/ui/drawing.h"
#include "app.h"
#include "icons.h"
#include "paths.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static App app;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} surface;

int main(int argc, char **argv)
{
    assert(argc == 2);
    app_init(&app, argv[1], true, true);
    strcpy(app.theme_id, "glass");
    themes_init(&app);
    assert(!strcmp(app.themes.active.id, "glass"));
    assert(app.themes.active.component_tiled);
    const uint8_t *coverage = themes_component_alpha(&app.themes.active, THEME_ROW_SELECTED);
    assert(coverage && !themes_component_alpha(&app.themes.active, THEME_HEADER));
    for (unsigned i = 0; i < 1024; i++)
        assert(((coverage[i / 4] >> ((i % 4) * 2)) & 3) == (i % 32) / 8);
    surface.before = surface.after = 0xc0ffee;
    for (unsigned i = 0; i < 256 * 192; i++)
        surface.pixels[i] = 0xfc00; /* Opaque blue under the red panel. */
    assert(draw_theme_component(&app.themes.active, surface.pixels, THEME_ROW_SELECTED, 10, 10, 32,
                                32));
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            unsigned alpha = x / 8;
            uint16_t expected =
                0x8000 | ((31 * alpha + 1) / 3) | (((31 * (3 - alpha) + 1) / 3) << 10);
            assert(surface.pixels[(y + 10) * 256 + x + 10] == expected);
        }
    /* Tile pitch remains native-sized while corner artwork stays attached. */
    assert(draw_theme_component(&app.themes.active, surface.pixels, THEME_HEADER, 10, 10, 80, 60));
    const uint16_t *header = themes_component(&app.themes.active, THEME_HEADER);
    assert(surface.pixels[10 * 256 + 10] == header[0]);
    assert(surface.pixels[69 * 256 + 89] == header[1023]);
    assert(surface.pixels[20 * 256 + 18] == surface.pixels[20 * 256 + 34]);
    assert(surface.pixels[18 * 256 + 20] == surface.pixels[34 * 256 + 20]);
    assert(draw_theme_component(&app.themes.active, surface.pixels, THEME_HEADER, -7, -5, 80, 60));
    assert(surface.pixels[0] == header[5 * 32 + 7]);
    assert(surface.pixels[5 * 256 + 1] == surface.pixels[5 * 256 + 17]);
    app.themes.active.component_tiled = false;
    assert(draw_theme_component(&app.themes.active, surface.pixels, THEME_HEADER, 10, 10, 80, 60));
    assert(surface.pixels[20 * 256 + 18] != surface.pixels[20 * 256 + 34]);
    for (int tiled = 0; tiled < 2; tiled++) {
        app.themes.active.component_tiled = tiled;
        for (unsigned border = 0; border <= 12; border++) {
            app.themes.active.component_border = border;
            for (int size = 1; size <= 80; size++)
                assert(draw_theme_component(&app.themes.active, surface.pixels, THEME_ROW_SELECTED,
                                            -15, 180, size, size));
        }
    }
    assert(surface.before == 0xc0ffee && surface.after == 0xc0ffee);
    char path[PATH_CAP];
    assert(path_preferences_file(path, app.root, "themes/glass/components/row-selected.png"));
    uint16_t native[1024];
    assert(icon_load_png(path, native));
    assert(native[8] == 0 && native[16] == 0x801f); /* Icons keep binary alpha. */
    assert(path_preferences_file(path, app.root, "themes/glass/theme.ini"));
    FILE *file = fopen(path, "w");
    assert(file && fputs("[theme]\nversion=1\n[components]\ndirectory=components\n", file) >= 0);
    assert(!fclose(file));
    themes_open(&app);
    assert(app.themes.ready && !themes_component_alpha(&app.themes.candidate, THEME_ROW_SELECTED));
    assert(!app.themes.candidate.component_tiled);
    assert(themes_component_alpha(&app.themes.active, THEME_ROW_SELECTED));
    themes_cancel(&app);
    assert(themes_component_alpha(&app.themes.active, THEME_ROW_SELECTED));
    assert(app.themes.active.component_tiled);
    themes_open(&app);
    themes_apply(&app);
    assert(!themes_component_alpha(&app.themes.active, THEME_ROW_SELECTED));
    assert(!app.themes.active.component_tiled);
    puts("PASS: four-level component coverage, opaque fast path, clipped slices and preview "
         "isolation");
}
