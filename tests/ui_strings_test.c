#include "app.h"
#include "association_editor.h"
#include "cheats.h"
#include "dashboard.h"
#include "font_pack.h"
#include "game_options.h"
#include "paths.h"
#include "picker.h"
#include "renderer.h"
#include "settings.h"
#include "storage_state.h"
#include "tabs.h"
#include "themes.h"
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
static uint16_t start_row[256 * FONT_HEIGHT], moving_row[256 * FONT_HEIGHT];
static uint16_t unselected_row[256 * FONT_HEIGHT];
static bool same_region(const uint16_t *a, const uint16_t *b, int x, int width)
{
    for (int y = 0; y < FONT_HEIGHT; y++)
        if (memcmp(a + y * 256 + x, b + y * 256 + x, (size_t)width * sizeof *a))
            return false;
    return true;
}
static void game_row(uint16_t *row)
{
    top.before = top.after = bottom.before = bottom.after = 0xA11CE;
    game_editor_render(&app, top.pixels, bottom.pixels);
    assert(top.before == 0xA11CE && top.after == 0xA11CE && bottom.before == 0xA11CE &&
           bottom.after == 0xA11CE);
    memcpy(row, bottom.pixels + 152 * 256, sizeof start_row);
}
static void title_options_marquee(const char *root)
{
    catalog(root, "version=1\ndialog.game_hotkey_label=Button combination for the in-game menu\n");
    ui_language_load(&app.language, root, true);
    assert(app.language.loaded);
    const FontPack *previous = font_current();
    static FontPack proportional;
    font_builtin(&proportional);
    proportional.proportional = true;
    proportional.width = 9;
    memset(proportional.advance, 9, sizeof proportional.advance);
    for (unsigned face = 0; face < 2; face++) {
        const FontPack *font = face ? &proportional : NULL;
        font_use(font);
        app.frame_count = 100;
        app.reduced_motion = false;
        game_editor_open(&app, true);
        app.game_editor.defaults = false;
        game_options_defaults(&app.game_editor.options, true);
        game_options_defaults(&app.game_editor.inherited, false);
        for (int i = 0; i < GAME_HOTKEY; i++) {
            app.frame_count++;
            game_editor_move(&app, 1);
        }
        assert(app.game_editor.selected == GAME_HOTKEY && app.game_editor.scroll == 2);
        unsigned selected_at = app.frame_count;
        game_row(start_row);
        memcpy(unselected_row, bottom.pixels + 134 * 256, sizeof unselected_row);
        app.frame_count += 30;
        game_row(moving_row);
        assert(!memcmp(unselected_row, bottom.pixels + 134 * 256, sizeof unselected_row));
        assert(!same_region(start_row, moving_row, 14, 110));
        assert(!same_region(start_row, moving_row, 132, 112));
        assert(same_region(start_row, moving_row, 124, 8)); /* Empty column gutter. */
        assert(same_region(start_row, moving_row, 244, 12));
        const char *label = ui_translate(&app.language, "dialog.game_hotkey_label", "missing");
        unsigned cycle = 3 * (font_measure(font, label) + 3 * font_advance(font, ' '));
        app.frame_count = selected_at + cycle;
        game_row(moving_row);
        assert(same_region(start_row, moving_row, 14, 110)); /* Continuous loop repeats. */
        app.frame_count += 17;
        game_editor_move(&app, -1);
        game_editor_move(&app, 1);
        game_row(moving_row);
        assert(same_region(start_row, moving_row, 14, 110) &&
               same_region(start_row, moving_row, 132, 112));
        app.frame_count += 17;
        game_editor_touch(&app, 20, 42);
        game_editor_touch(&app, 20, 150);
        game_row(moving_row);
        assert(same_region(start_row, moving_row, 14, 110) &&
               same_region(start_row, moving_row, 132, 112));
        app.frame_count = UINT32_MAX - 2;
        game_editor_move(&app, -1);
        game_editor_move(&app, 1);
        game_row(start_row);
        app.frame_count = 27; /* Thirty elapsed frames across the counter wrap. */
        game_row(moving_row);
        assert(!same_region(start_row, moving_row, 14, 110));
        assert(!same_region(start_row, moving_row, 132, 112));
        game_editor_adjust(&app, 1);
        assert(app.game_editor.text_started_frame == app.frame_count);
        app.reduced_motion = true;
        game_row(start_row);
        app.frame_count += 30;
        game_row(moving_row);
        assert(!memcmp(start_row, moving_row, sizeof start_row));
        app.game_editor.open = false;
    }
    app.reduced_motion = false;
    font_use(previous);
}
int main(int argc, char **argv)
{
    assert(argc == 2 && path_prepare_preferences_directory(argv[1]));
    assert(sizeof(UiLanguage) < 48000); /* More keys without larger fixed per-value buffers. */
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
    catalog(argv[1], "version=1\nvalue.off=No\nvalue.grid=Mosaico\n");
    ui_language_load(&app.language, argv[1], true);
    for (int page = 0; page < settings_section_count(&app); page++) {
        for (int row = 0; row < settings_option_count(&app, page); row++) {
            const SettingsOption *option = settings_option(&app, page, row);
            char value[96];
            if (!strcmp(option->label, "Auto-launch recent")) {
                app.auto_launch_delay = 0;
                settings_value(&app, option, value, sizeof value);
                assert(!strcmp(value, "No"));
            }
            if (!strcmp(option->label, "Theme")) {
                strcpy(app.themes.active.name, "Off");
                settings_value(&app, option, value, sizeof value);
                assert(!strcmp(value, "Off")); /* Never translate a personal name. */
            }
            if (!strcmp(option->label, "Browser view")) {
                app.browser_style = BROWSER_GRID;
                settings_value(&app, option, value, sizeof value);
                assert(!strcmp(value, "Mosaico"));
            }
        }
    }
    const char *catalogs[] = {"config/languages/template.ini",
                              "config/languages/spanish.ini",
                              "config/languages/japanese.ini",
                              "config/languages/french.ini",
                              "config/languages/german.ini",
                              "config/languages/italian.ini",
                              "config/languages/portuguese-brazilian.ini",
                              "config/languages/dutch.ini"};
    for (unsigned i = 0; i < sizeof catalogs / sizeof *catalogs; i++) {
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
        static CheatState cheat;
        void (*dialogs[])(const App *, uint16_t *, uint16_t *) = {association_editor_render,
                                                                  cheats_render,
                                                                  dashboard_render,
                                                                  game_editor_render,
                                                                  picker_render,
                                                                  storage_render,
                                                                  themes_render};
        app.cheats = &cheat;
        for (int layout = 0; layout < 4; layout++) {
            app.themes.active.top_layout = layout;
            for (unsigned dialog = 0; dialog < sizeof dialogs / sizeof *dialogs; dialog++) {
                top.before = top.after = bottom.before = bottom.after = 0xA11CE;
                dialogs[dialog](&app, top.pixels, bottom.pixels);
                assert(top.before == 0xA11CE && top.after == 0xA11CE);
                assert(bottom.before == 0xA11CE && bottom.after == 0xA11CE);
            }
        }
        app.cheats = NULL;
        for (int panel = PANEL_QUICK; panel <= PANEL_ACTIONS; panel++) {
            app.panel = panel;
            int count = panel == PANEL_QUICK ? QUICK_CONTROL_COUNT : ACTION_COUNT;
            for (int row = 0; row < count; row++) {
                app.panel_selection = row;
                app.panel_scroll = row;
                top.before = top.after = bottom.before = bottom.after = 0xA11CE;
                render_app(&app, top.pixels, bottom.pixels, "12:00");
                assert(top.before == 0xA11CE && top.after == 0xA11CE);
                assert(bottom.before == 0xA11CE && bottom.after == 0xA11CE);
            }
        }
        app.panel = PANEL_NONE;
        assert(strcmp(ui_translate(&app.language, "dialog.game_slot.help", "missing"), "missing"));
        assert(strcmp(ui_translate(&app.language, "dialog.sd_removed", "missing"), "missing"));
    }
    char many[24000] = "version=1\n";
    for (unsigned i = 0; i <= UI_EXTRA_CAP; i++) {
        char record[64];
        snprintf(record, sizeof record, "value.fixture_%u=Texto %u\n", i, i);
        strcat(many, record);
        if (i + 1 == UI_EXTRA_CAP) {
            catalog(argv[1], many);
            ui_language_load(&app.language, argv[1], true);
            assert(app.language.loaded && app.language.extra_count == UI_EXTRA_CAP);
            assert(!strcmp(ui_translate(&app.language, "value.fixture_255", "bad"), "Texto 255"));
            assert(!strcmp(ui_translate(&app.language, "value.fixture_0", "bad"), "Texto 0"));
            assert(!strcmp(ui_translate(&app.language, "value.fixture_383", "bad"), "Texto 383"));
        }
    }
    catalog(argv[1], many);
    ui_language_load(&app.language, argv[1], true);
    assert(!app.language.loaded);
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
    catalog(argv[1], "\xef\xbb\xbfversion=1\r\ngames=Jeux\r\n");
    ui_language_load(&app.language, argv[1], true);
    assert(app.language.loaded && !strcmp(ui_text(&app.language, UI_GAMES), "Jeux"));
    char binary_path[PATH_CAP];
    assert(path_preferences_file(binary_path, argv[1], "language.ini"));
    const char binary_catalog[] = "version=1\ngames=Jeux\0hidden";
    FILE *binary = fopen(binary_path, "wb");
    assert(binary &&
           fwrite(binary_catalog, 1, sizeof binary_catalog - 1, binary) ==
               sizeof binary_catalog - 1 &&
           !fclose(binary));
    ui_language_load(&app.language, argv[1], true);
    assert(!app.language.loaded);
    title_options_marquee(argv[1]);
    catalog(argv[1], "version=1\ngames=Jeux\n");
    app_init_mode(&app, argv[1], true, true, true);
    assert(!app.language.loaded && !strcmp(tabs_label(&app, CATEGORY_GAMES), "Games"));
    puts("PASS: partial UTF-8 UI catalog, English fallback, malformed catalog rejection and "
         "recovery bypass");
}
