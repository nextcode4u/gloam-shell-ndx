#include "../arm9/source/ui/drawing.h"
#include "app.h"
#include "font_pack.h"
#include "paths.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static App app;
static FontPack font, previous;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} screen;
static uint16_t expected[256 * 192];

static void fill(void)
{
    screen.before = screen.after = 0xdecaf;
    for (unsigned i = 0; i < 256 * 192; i++)
        screen.pixels[i] = expected[i] = 0x8421;
}
static void configuration(const char *path, const char *value)
{
    FILE *file = fopen(path, "w");
    assert(file && fputs(value, file) >= 0 && !fclose(file));
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    app_init(&app, argv[1], true, true);
    strcpy(app.theme_id, "type");
    themes_init(&app);
    assert(!strcmp(app.themes.active.id, "type") && app.themes.active.custom_heading);
    assert(app.themes.active.font.height == 12 && app.themes.active.heading_font.height == 16);
    font = app.themes.active.font;
    font_use(&font);
    assert(draw_text_width("WiWi") == 26 && draw_text_width("iiiiiiiiii") == 30);
    assert(font_advance(&font, 0xE9) == 7 && font_advance(&font, 0x391) == 7);
    for (unsigned frame = 0; frame < 220; frame++) {
        fill();
        int offset = (frame / 3) % 35;
        draw_text(expected, 20 - offset, 30, "WiWi", 0xffff, 20);
        draw_text(expected, 20 - offset + 35, 30, "WiWi", 0xffff, 20);
        draw_text_scrolling(screen.pixels, 20, 30, "WiWi", 0xffff, 13, frame, true);
        for (int y = 0; y < 192; y++)
            for (int x = 0; x < 256; x++)
                assert(screen.pixels[y * 256 + x] ==
                       (x >= 20 && x < 33 ? expected[y * 256 + x] : 0x8421));
        assert(screen.before == 0xdecaf && screen.after == 0xdecaf);
    }
    fill();
    draw_text(expected, 12, 30, "iiiiiiiiii", 0xffff, 5);
    draw_text_clipped(screen.pixels, 12, 30, "iiiiiiiiii", 0xffff, 5);
    assert(!memcmp(screen.pixels, expected, sizeof expected));
    assert(screen.pixels[32 * 256 + 39] == 0xffff); /* All ten narrow letters fit. */
    fill();
    draw_text_wrapped(screen.pixels, 12, 30, "WW iiii", 0xffff, 3, 2);
    draw_text(expected, 12, 30, "WW", 0xffff, 3);
    draw_text(expected, 12, 42, "iiii", 0xffff, 3);
    assert(!memcmp(screen.pixels, expected, sizeof expected));
    fill();
    draw_heading(&app.themes.active, screen.pixels, 10, 7, "WWWWWWWW", 0xffff, 25);
    for (int y = 0; y < 192; y++)
        for (int x = 0; x < 256; x++)
            if (x < 10 || x >= 35 || y < 5 || y >= 21)
                assert(screen.pixels[y * 256 + x] == 0x8421);
    draw_heading_scrolling(&app.themes.active, screen.pixels, 250, 185, "WiWi", 0xffff, 15, 1234,
                           true);
    assert(screen.before == 0xdecaf && screen.after == 0xdecaf);

    char path[PATH_CAP];
    assert(path_preferences_file(path, app.root, "themes/type/theme.ini"));
    configuration(path, "[theme]\nversion=1\n[fonts]\nui=body.dsfont\n");
    themes_init(&app);
    assert(!*app.themes.active.id); /* Modern metrics require an explicit format version. */
    configuration(path, "[theme]\nversion=2\n[fonts]\nui=heading.dsfont\n");
    themes_init(&app);
    assert(!*app.themes.active.id); /* Tall text cannot alter body/touch geometry. */
    assert(path_preferences_file(path, app.root, "themes/type/body.dsfont"));
    previous = font;
    FILE *file = fopen(path, "r+b");
    assert(file && !fseek(file, 16, SEEK_SET) && fputc(0, file) == 0 && !fclose(file));
    assert(!font_pack_load(&font, path) && !memcmp(&font, &previous, sizeof font));
    puts("PASS: variable advances, pixel wrapping/looping/clipping, heading bounds and metric "
         "rejection");
}
