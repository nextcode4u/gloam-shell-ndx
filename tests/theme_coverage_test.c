#include "../arm9/source/ui/drawing.h"
#include "app.h"
#include "icons.h"
#include "image_decode.h"
#include "paths.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static App app;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} surface;

static uint16_t expected_material(unsigned shade)
{
    int delta = (int)shade - 8;
    return 0x8000 | (8 + delta) | ((12 + delta) << 5) | ((16 + delta) << 10);
}

static void material_surfaces(void)
{
    strcpy(app.theme_id, "material");
    themes_init(&app);
    assert(!strcmp(app.themes.active.id, "material"));
    char source_path[PATH_CAP];
    assert(path_preferences_file(source_path, app.root, "themes/material/normal.png"));
    FILE *source_file = fopen(source_path, "rb");
    assert(source_file && !fseek(source_file, 0, SEEK_END));
    long bytes = ftell(source_file);
    assert(bytes > 24 && !fseek(source_file, 0, SEEK_SET));
    unsigned char *encoded = malloc((size_t)bytes);
    assert(encoded && fread(encoded, 1, (size_t)bytes, source_file) == (size_t)bytes);
    assert(!fclose(source_file));
    static struct {
        uint32_t before;
        uint8_t pixels[256 * 192 / 2];
        uint32_t after;
    } packed;
    packed.before = packed.after = 0xabcdef;
    assert(image_decode_material(packed.pixels, encoded, (size_t)bytes));
    assert(packed.pixels[0] == 0 && packed.pixels[127] == 0xfe);
    assert(!image_decode_material(NULL, encoded, (size_t)bytes));
    assert(!image_decode_material(packed.pixels, encoded, 23));
    assert(!image_decode_material(packed.pixels, encoded, 256 * 1024 + 1));
    encoded[19] = 255; /* Wrong dimensions reject before decompression. */
    assert(!image_decode_material(packed.pixels, encoded, (size_t)bytes));
    free(encoded);
    assert(packed.before == 0xabcdef && packed.after == 0xabcdef);
    for (unsigned i = 0; i < 256 * 192; i++)
        surface.pixels[i] = 0xfc00;
    assert(draw_theme_component(&app.themes.active, surface.pixels, THEME_ROW, 0, 0, 256, 192));
    assert(surface.pixels[0] == 0xfc00);   /* The transparent corner remains a hole. */
    assert(surface.pixels[257] == 0x801f); /* The red corner ornament is not recolored. */
    assert(surface.pixels[50 * 256 + 50] == expected_material(50 * 15 / 255));
    assert(surface.pixels[50 * 256 + 66] == expected_material(66 * 15 / 255));
    assert(surface.pixels[50 * 256 + 50] != surface.pixels[50 * 256 + 66]);
    assert(draw_theme_component(&app.themes.active, surface.pixels, THEME_ROW, -10, -5, 300, 210));
    assert(surface.pixels[50 * 256 + 50] == expected_material(50 * 15 / 255));
    assert(draw_theme_component(&app.themes.active, surface.pixels, THEME_ROW_SELECTED, 0, 0, 256,
                                192));
    assert(surface.pixels[50 * 256 + 50] == expected_material(15 - 50 * 15 / 191));
    const uint16_t *palette;
    const uint8_t *active = themes_component_material(&app.themes.active, THEME_ROW, &palette);
    assert(active && palette[8] == expected_material(8));
    assert(!themes_component_material(&app.themes.active, THEME_BATTERY_FULL, &palette) &&
           !palette);
    char path[PATH_CAP];
    assert(path_preferences_file(path, app.root, "themes/material/theme.ini"));
    FILE *file = fopen(path, "w");
    assert(file && fputs("[theme]\nversion=2\n[components]\ndirectory=components\n"
                         "material=neutral.png\nfocus_material=invalid.png\n",
                         file) >= 0);
    assert(!fclose(file));
    themes_open(&app);
    themes_preview(&app);
    assert(app.themes.ready);
    const uint8_t *candidate =
        themes_component_material(&app.themes.candidate, THEME_ROW_SELECTED, &palette);
    assert(candidate && candidate[0] == 0x88); /* Invalid focus image falls back to normal. */
    assert(themes_component_material(&app.themes.active, THEME_ROW, &palette) == active);
    themes_cancel(&app);
    assert(themes_component_material(&app.themes.active, THEME_ROW, &palette) == active);
    themes_open(&app);
    themes_preview(&app);
    themes_apply(&app);
    assert(themes_component_material(&app.themes.active, THEME_ROW, &palette)[0] == 0x88);
    assert(surface.before == 0xc0ffee && surface.after == 0xc0ffee);
}

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
    themes_preview(&app);
    assert(app.themes.ready && !themes_component_alpha(&app.themes.candidate, THEME_ROW_SELECTED));
    assert(!app.themes.candidate.component_tiled);
    assert(themes_component_alpha(&app.themes.active, THEME_ROW_SELECTED));
    themes_cancel(&app);
    assert(themes_component_alpha(&app.themes.active, THEME_ROW_SELECTED));
    assert(app.themes.active.component_tiled);
    themes_open(&app);
    themes_preview(&app);
    themes_apply(&app);
    assert(!themes_component_alpha(&app.themes.active, THEME_ROW_SELECTED));
    assert(!app.themes.active.component_tiled);
    material_surfaces();
    puts("PASS: four-level component coverage, opaque fast path, clipped slices and preview "
         "isolation");
}
