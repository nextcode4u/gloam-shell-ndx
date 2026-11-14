#include "../arm9/source/ui/drawing.h"
#include "app.h"
#include "font_pack.h"
#include "layout.h"
#include "paths.h"
#include "preferences.h"
#include "renderer.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app, restored, recovery;
static uint16_t top[256 * 192], bottom[256 * 192];
static void write_text(const char *root, const char *relative, const char *text)
{
    char path[PATH_CAP];
    assert(path_join(path, root, relative));
    FILE *file = fopen(path, "w");
    assert(file && fputs(text, file) >= 0 && !fclose(file));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    assert(themes_battery_component(-1, false) == THEME_BATTERY_UNKNOWN);
    assert(themes_battery_component(16, false) == THEME_BATTERY_UNKNOWN);
    assert(themes_battery_component(0, false) == THEME_BATTERY_EMPTY);
    assert(themes_battery_component(1, false) == THEME_BATTERY_LOW);
    assert(themes_battery_component(7, false) == THEME_BATTERY_HALF);
    assert(themes_battery_component(11, false) == THEME_BATTERY_HIGH);
    assert(themes_battery_component(15, false) == THEME_BATTERY_FULL);
    assert(themes_battery_component(0, true) == THEME_BATTERY_CHARGING);
    app_init(&app, root, true, true);
    /* The notification replaces only the wireless slot; labels/battery keep their bounds. */
    static uint16_t ordinary_status[256 * 192];
    memset(top, 0, sizeof top);
    draw_theme_status(&app, top);
    memcpy(ordinary_status, top, sizeof top);
    app.updates.available = true;
    draw_theme_status(&app, top);
    unsigned changed = 0;
    for (int y = 0; y < 192; y++)
        for (int x = 0; x < 256; x++) {
            if (x < 176 || x >= 192 || y < 4 || y >= 20)
                assert(top[y * 256 + x] == ordinary_status[y * 256 + x]);
            else
                changed += top[y * 256 + x] != ordinary_status[y * 256 + x];
        }
    assert(changed > 0);
    app.updates.available = false;

    char manifest[PATH_CAP];
    assert(path_join(manifest, root, "_nds/dsi-frontend/themes/copper/theme.ini"));
    FILE *layout = fopen(manifest, "a");
    assert(layout &&
           fputs("\n[layout]\nview=grid\nicon_size=large\nrow_padding=6\nlarge_grid_columns=3\ntop_"
                 "layout=sidebar_left\nchrome=minimal\ncontent_opacity=40\n[colors]\nselection_"
                 "text=#ffffff\ntab_text=#000000\n",
                 layout) >= 0 &&
           !fclose(layout));
    ThemePackage original = app.themes.active;
    assert(!themes_icon(&original, PLACEHOLDER_FOLDER));
    themes_open(&app);
    assert(app.themes.count == 4 && app.themes.ready);
    themes_move(&app, 1);
    assert(!strcmp(app.themes.candidate.id, "copper") && app.themes.candidate.external_font);
    assert(!strcmp(app.themes.candidate.header, "COPPERWORKS"));
    assert(!memcmp(&original, &app.themes.active, sizeof original) && !app.preferences_dirty);
    assert(memcmp(&original.font, &app.themes.candidate.font, sizeof original.font));
    assert(app.themes.candidate.top_layout == 1 && app.themes.candidate.minimal_chrome);
    assert(app.themes.candidate.content_opacity == 40);
    assert(themes_selection_text(&app.themes.candidate) == 0xffff);
    assert(themes_tab_text(&app.themes.candidate) == 0x8000);
    const uint16_t *tab_home = themes_icon(&app.themes.candidate, THEME_TAB_ICON_FIRST);
    assert(tab_home && !themes_icon(&original, THEME_TAB_ICON_FIRST));
    memset(top, 0, sizeof top);
    assert(draw_theme_icon_asset(&app.themes.candidate, top, 4, 4, 16, THEME_TAB_ICON_FIRST));
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            uint16_t pixel = tab_home[(y * 2) * 32 + x * 2];
            assert(top[(y + 4) * 256 + x + 4] == ((pixel & 0x8000) ? pixel : 0));
        }
    assert(!draw_theme_icon_asset(&original, top, 4, 4, 16, THEME_TAB_ICON_FIRST));
    assert(!draw_theme_icon_asset(&app.themes.candidate, top, 4, 4, 16, THEME_ICON_COUNT));
    const uint16_t *component = themes_component(&app.themes.candidate, THEME_ROW_SELECTED);
    assert(component && !themes_component(&original, THEME_ROW_SELECTED));
    uint16_t expected_component[32 * 32];
    memcpy(expected_component, component, sizeof expected_component);
    memset(top, 0, sizeof top);
    assert(draw_theme_component(&app.themes.candidate, top, THEME_ROW_SELECTED, 0, 0, 32, 32));
    for (unsigned y = 0; y < 32; y++)
        for (unsigned x = 0; x < 32; x++)
            assert(top[y * 256 + x] == expected_component[y * 32 + x]);
    struct {
        uint32_t before;
        uint16_t pixels[256 * 192];
        uint32_t after;
    } guard;
    for (unsigned border = 0; border <= 12; border++) {
        app.themes.candidate.component_border = border;
        for (int size = 1; size <= 65; size++) {
            guard.before = guard.after = 0xC0FFEE;
            memset(guard.pixels, 0, sizeof guard.pixels);
            assert(draw_theme_component(&app.themes.candidate, guard.pixels, THEME_ROW_SELECTED,
                                        -10, 180, size, size));
            assert(draw_theme_component(&app.themes.candidate, guard.pixels, THEME_ROW_SELECTED, 0,
                                        0, size, size));
            assert(guard.before == 0xC0FFEE && guard.after == 0xC0FFEE);
        }
    }
    app.themes.candidate.component_border = 8;
    memset(top, 0, sizeof top);
    assert(draw_theme_component(&app.themes.candidate, top, THEME_ROW_SELECTED, 0, 0, 32, 2));
    assert(top[8] == expected_component[8]);
    assert(top[256 + 8] == expected_component[31 * 32 + 8]);
    const uint16_t *folder = themes_icon(&app.themes.candidate, PLACEHOLDER_FOLDER);
    assert(folder && themes_icon(&app.themes.candidate, PLACEHOLDER_COUNT + 6));
    uint16_t expected[32 * 32];
    memcpy(expected, folder, sizeof expected);
    assert(!themes_icon(&original, PLACEHOLDER_FOLDER));
    memset(top, 0, sizeof top);
    draw_theme_icon(&app.themes.candidate, top, 0, 0, 32, PLACEHOLDER_FOLDER, 0xffff);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            assert(top[y * 256 + x] == expected[y * 32 + x]);
    draw_theme_icon(&app.themes.candidate, top, -10, 180, 64, PLACEHOLDER_FOLDER, 0xffff);
    render_app(&app, top, bottom, "12:00");
    themes_cancel(&app);
    assert(!app.themes.open && !*app.theme_id);
    assert(!themes_icon(&app.themes.candidate, PLACEHOLDER_FOLDER));
    assert(!themes_component(&app.themes.candidate, THEME_ROW_SELECTED));
    assert(font_row('A', 3) == original.font.rows['A' - 32][3]);
    themes_open(&app);
    themes_move(&app, 1);
    themes_apply(&app);
    assert(!app.themes.open && !strcmp(app.theme_id, "copper") && app.preferences_dirty);
    assert(!memcmp(themes_icon(&app.themes.active, PLACEHOLDER_FOLDER), expected, sizeof expected));
    assert(!memcmp(themes_component(&app.themes.active, THEME_ROW_SELECTED), expected_component,
                   sizeof expected_component));
    write_text(root, "_nds/dsi-frontend/themes/copper/components/row-selected.png", "broken PNG");
    /* A bad or absent symbol must not reject the package or poison other icons. */
    write_text(root, "_nds/dsi-frontend/themes/copper/icons/folder.png", "broken PNG");
    assert(path_join(manifest, root, "_nds/dsi-frontend/themes/copper/icons/file.png"));
    assert(!remove(manifest));
    themes_open(&app);
    assert(app.themes.ready);
    assert(!themes_component(&app.themes.candidate, THEME_ROW_SELECTED));
    assert(themes_component(&app.themes.candidate, THEME_HEADER));
    assert(!themes_icon(&app.themes.candidate, PLACEHOLDER_FOLDER));
    assert(!themes_icon(&app.themes.candidate, PLACEHOLDER_FILE));
    assert(themes_icon(&app.themes.candidate, PLACEHOLDER_AUDIO));
    themes_cancel(&app);
    assert(!memcmp(themes_icon(&app.themes.active, PLACEHOLDER_FOLDER), expected, sizeof expected));
    assert(app.themes.active.palette.bg != original.palette.bg);
    assert(app.browser_style == BROWSER_GRID && app.icon_size == ICON_LARGE);
    assert(layout_grid_columns(&app) == 3 && layout_row_height(&app) == 58);
    assert(layout_visible_rows(&app) == 6); /* Home grid keeps two rows of three. */
    render_app(&app, top, bottom, "12:00");
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(!strcmp(restored.theme_id, "copper") && restored.themes.active.external_font);
    assert(!memcmp(&app.themes.active.font, &restored.themes.active.font, sizeof original.font));
    themes_open(&restored);
    themes_move(&restored, 2); /* copper -> invalid package */
    assert(!restored.themes.ready);
    themes_apply(&restored);
    assert(restored.themes.open && !strcmp(restored.theme_id, "copper"));
    themes_cancel(&restored);
    assert(font_row('A', 3) == restored.themes.active.font.rows['A' - 32][3]);
    restored.accent_index = 2;
    themes_accent_override(&restored);
    restored.icon_size = ICON_LARGE;
    restored.browser_style = BROWSER_LIST;
    restored.reduced_motion = false;
    restored.clock_12_hour = true;
    assert(app_save(&restored));
    app_init_mode(&recovery, root, true, true, true);
    assert(recovery.recovery_mode && recovery.category == CATEGORY_HOME);
    assert(recovery.icon_size == ICON_SMALL && recovery.browser_style == BROWSER_CLASSIC);
    assert(recovery.reduced_motion && !recovery.show_covers &&
           !recovery.themes.active.external_font);
    recovery.clock_12_hour = false;
    assert(!strcmp(recovery.theme_id, "copper"));
    app_adjust_control(&recovery, QUICK_ACCENT, 1);
    app_move(&recovery, 1);
    assert(app_save(&recovery));
    app_init(&restored, root, true, true);
    assert(restored.accent_index == 2 && restored.theme_accent_override);
    assert(restored.icon_size == ICON_LARGE && restored.browser_style == BROWSER_LIST);
    assert(!restored.reduced_motion && restored.themes.active.external_font);
    assert(restored.show_covers && restored.clock_12_hour);
    char path[PATH_CAP];
    assert(path_join(path, root, "_nds/dsi-frontend/themes/copper/fonts/ui.dsfont"));
    FontPack valid, untouched;
    assert(font_pack_load(&valid, path));
    untouched = valid;
    FILE *file = fopen(path, "r+b");
    assert(file && !fseek(file, 16, SEEK_SET));
    assert(fputc(255, file) == 255 && fputc(255, file) == 255 && !fclose(file));
    assert(!font_pack_load(&valid, path));
    assert(!memcmp(&valid, &untouched, sizeof valid));
    app_init_mode(&recovery, root, true, true, true);
    assert(!*recovery.themes.status && !*recovery.themes.active.id); /* Skips the broken file. */
    app_init(&restored, root, true, true);
    assert(*restored.themes.status && restored.panel == PANEL_MESSAGE);
    assert(!strcmp(restored.theme_id, "copper") && !*restored.themes.active.id);
    assert(app_save(&restored)); /* A missing package must not erase the requested ID. */
    app_init(&app, root, true, true);
    assert(!strcmp(app.theme_id, "copper"));
    write_text(root, "_nds/dsi-frontend/themes/zzzbad/theme.ini",
               "[theme]\nversion=1\n[layout]\nlarge_grid_columns=5\n");
    themes_open(&app);
    themes_move(&app, 3);
    assert(!app.themes.ready);
    themes_cancel(&app);
    const char *bad_layout[] = {"top_layout=unknown",
                                "top_layout=spotlight",
                                "top_layout=gallery",
                                "top_layout=journal",
                                "tabs_position=top",
                                "chrome=off",
                                "content_opacity=101",
                                "top_content_opacity=50",
                                "bottom_content_opacity=50",
                                "content_opacity=-1",
                                "scene_cover_x=240",
                                "scene_cover_height=91",
                                "scene_cover_y=0",
                                "tab_style=unknown",
                                "tab_highlight=unknown",
                                "tab_gap=9",
                                "tab_gap=-1",
                                "tab_visible=0",
                                "tab_visible=6"};
    for (unsigned i = 0; i < sizeof bad_layout / sizeof *bad_layout; i++) {
        char content[160];
        snprintf(content, sizeof content, "[theme]\nversion=1\n[layout]\n%s\n", bad_layout[i]);
        write_text(root, "_nds/dsi-frontend/themes/zzzbad/theme.ini", content);
        themes_open(&app);
        themes_move(&app, 3);
        assert(!app.themes.ready);
        themes_cancel(&app);
    }
    const char *compositions[] = {"spotlight", "gallery", "journal"};
    for (unsigned i = 0; i < 3; i++) {
        char content[160];
        snprintf(content, sizeof content,
                 "[theme]\nversion=2\n[layout]\n"
                 "tabs_position=top\ntop_layout=%s\n",
                 compositions[i]);
        write_text(root, "_nds/dsi-frontend/themes/zzzbad/theme.ini", content);
        themes_open(&app);
        themes_move(&app, 3);
        assert(app.themes.ready && app.themes.candidate.top_layout == 4 + i);
        assert(app.themes.candidate.tabs_position == 1);
        themes_cancel(&app);
    }
    const char *bad_positions[] = {"tabs_position=top\ntop_layout=scene",
                                   "tabs_position=left\ntop_layout=journal"};
    for (unsigned i = 0; i < 2; i++) {
        char content[160];
        snprintf(content, sizeof content, "[theme]\nversion=2\n[layout]\n%s\n", bad_positions[i]);
        write_text(root, "_nds/dsi-frontend/themes/zzzbad/theme.ini", content);
        themes_open(&app);
        themes_move(&app, 3);
        assert(!app.themes.ready);
        themes_cancel(&app);
    }
    const char *headers[] = {"", "My library"};
    for (unsigned i = 0; i < 2; i++) {
        char content[160];
        snprintf(content, sizeof content, "[theme]\nversion=1\nheader=%s\n", headers[i]);
        write_text(root, "_nds/dsi-frontend/themes/zzzbad/theme.ini", content);
        themes_open(&app);
        themes_move(&app, 3);
        assert(app.themes.ready && !strcmp(app.themes.candidate.header, headers[i]));
        themes_cancel(&app);
    }
    const char *bad_components[] = {"directory=../escape", "border=13", "border=-1", "border=bad"};
    for (unsigned i = 0; i < sizeof bad_components / sizeof *bad_components; i++) {
        char content[160];
        snprintf(content, sizeof content, "[theme]\nversion=1\n[components]\n%s\n",
                 bad_components[i]);
        write_text(root, "_nds/dsi-frontend/themes/zzzbad/theme.ini", content);
        themes_open(&app);
        themes_move(&app, 3);
        assert(!app.themes.ready);
        themes_cancel(&app);
    }
    write_text(root, "_nds/dsi-frontend/themes/zzzbad/theme.ini",
               "[theme]\nversion=2\n[layout]\ncontent_opacity=40\n"
               "top_content_opacity=0\nbottom_content_opacity=100\n");
    themes_open(&app);
    themes_move(&app, 3);
    assert(app.themes.ready && app.themes.candidate.format_version == 2);
    ThemePackage surfaces = app.themes.candidate;
    assert(themes_content_opacity(&surfaces, 0) == 0);
    assert(themes_content_opacity(&surfaces, 1) == 100);
    surfaces.background_strength = 100;
    for (int screen = 0; screen < 2; screen++)
        strcpy(surfaces.backgrounds[screen], "tests/fixtures/cover-example.png");
    themes_background_load(&surfaces, true);
    themes_background_draw(&surfaces, 0, top);
    themes_background_draw(&surfaces, 1, bottom);
    unsigned different = 0;
    for (int y = 4; y < 188; y++)
        for (int x = 6; x < 250; x++) {
            assert(bottom[y * 256 + x] == surfaces.palette.bg);
            different += top[y * 256 + x] != bottom[y * 256 + x];
        }
    assert(different > 0);
    themes_cancel(&app);
    puts("PASS: theme preview/apply/cancel, validated fonts/contrast, persistence and recovery "
         "isolation");
}
