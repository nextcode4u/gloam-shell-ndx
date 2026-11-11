#include "app.h"
#include "paths.h"
#include "renderer.h"
#include "settings.h"
#include "tabs.h"
#include "ui_strings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static App app;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} top, bottom;
static void catalog(const char *root, const char *text)
{
    char path[PATH_CAP];
    assert(path_preferences_file(path, root, "language.ini"));
    FILE *file = fopen(path, "wb");
    assert(file && fputs(text, file) >= 0 && !fclose(file));
}
int main(int argc, char **argv)
{
    assert(argc == 2 && path_prepare_preferences_directory(argv[1]));
    catalog(argv[1], "version=1\ngames=Jeux\nsettings=Réglages\n");
    app_init(&app, argv[1], true, true);
    assert(app.language.loaded && !strcmp(tabs_label(&app, CATEGORY_GAMES), "Jeux"));
    assert(!strcmp(ui_text(&app.language, UI_SETTINGS), "Réglages"));
    assert(!strcmp(ui_text(&app.language, UI_FILES), "Files"));
    catalog(argv[1], "version=1\nsetting.accent.label=Color\nsetting.accent.help=Elige un "
                     "color.\nvalue.off=No\n");
    ui_language_load(&app.language, argv[1], true);
    app.advanced_settings = true;
    const SettingsOption *accent = settings_option(&app, 0, 0);
    assert(app.language.loaded && !strcmp(settings_option_text(&app, accent, false), "Color"));
    assert(!strcmp(settings_option_text(&app, accent, true), "Elige un color."));
    assert(!strcmp(ui_translate(&app.language, "dialog.unknown", "English"), "English"));
    const char *catalogs[] = {"config/languages/template.ini", "config/languages/spanish.ini"};
    for (unsigned i = 0; i < 2; i++) {
        FILE *file = fopen(catalogs[i], "rb");
        char data[32769];
        assert(file);
        size_t bytes = fread(data, 1, sizeof data - 1, file);
        data[bytes] = 0;
        assert(!ferror(file) && !fclose(file));
        catalog(argv[1], data);
        ui_language_load(&app.language, argv[1], true);
        assert(app.language.loaded && app.language.extra_count > 60);
        settings_open(&app);
        for (int page = 0; page < settings_section_count(&app); page++) {
            app.settings.section = page;
            app.settings.selected = app.settings.scroll = 0;
            for (int row = 0; row < settings_option_count(&app, page); row++) {
                top.before = top.after = bottom.before = bottom.after = 0xA11CE;
                render_app(&app, top.pixels, bottom.pixels, "12:00");
                assert(top.before == 0xA11CE && top.after == 0xA11CE);
                assert(bottom.before == 0xA11CE && bottom.after == 0xA11CE);
                settings_move(&app, 1);
            }
        }
        settings_close(&app);
    }
    const char *bad[] = {"version=1\ngames=First\ngames=Second\n", "version=2\ngames=Jeux\n",
                         "version=1\nunknown=No\n",
                         "version=1\nsetting.accent.label=A\nsetting.accent.label=B\n",
                         "version=1\ngames=\xc0\xaf\n"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        catalog(argv[1], bad[i]);
        ui_language_load(&app.language, argv[1], true);
        assert(!app.language.loaded && !strcmp(ui_text(&app.language, UI_GAMES), "Games"));
    }
    char too_long[96] = "version=1\ngames=";
    size_t prefix = strlen(too_long);
    memset(too_long + prefix, 'A', 64);
    too_long[prefix + 64] = 0;
    catalog(argv[1], too_long);
    ui_language_load(&app.language, argv[1], true);
    assert(!app.language.loaded);
    catalog(argv[1], "version=1\ngames=Jeux\n");
    app_init_mode(&app, argv[1], true, true, true);
    assert(!app.language.loaded && !strcmp(tabs_label(&app, CATEGORY_GAMES), "Games"));
    puts("PASS: partial UTF-8 UI catalog, English fallback, malformed catalog rejection and "
         "recovery bypass");
}
