/* Exercise the shipped packages through the real loader, renderer and WAV reader. */
#include "app.h"
#include "appearance.h"
#include "audio.h"
#include "browser.h"
#include "layout.h"
#include "preferences.h"
#include "renderer.h"
#include "settings.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static App app, restored;
static uint16_t stationary_rows[256 * 192];
static struct {
    uint32_t before;
    uint16_t pixels[256 * 192];
    uint32_t after;
} top, bottom;

static void choose(const char *id)
{
    themes_open(&app);
    int index = 0;
    while (index < app.themes.count && strcmp(app.themes.ids[index], id))
        index++;
    assert(index < app.themes.count);
    themes_move(&app, index - app.themes.selected);
    assert(app.themes.ready);
}
static void draw(void)
{
    top.before = bottom.before = 0x12345678;
    top.after = bottom.after = 0x87654321;
    render_app(&app, top.pixels, bottom.pixels, "11:59 PM");
    assert(top.before == 0x12345678 && bottom.before == 0x12345678);
    assert(top.after == 0x87654321 && bottom.after == 0x87654321);
}

static int luminance(uint16_t color)
{
    return (3 * (color & 31) + 6 * ((color >> 5) & 31) + ((color >> 10) & 31)) / 10;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *ids[] = {"folio", "nocturne", "tide",    "phosphor", "eclipse",
                         "prism", "ember",    "glacier", "garnet"};
    int views[] = {BROWSER_LIST, BROWSER_EXTENDED, BROWSER_GRID, BROWSER_CLASSIC, BROWSER_GRID,
                   BROWSER_GRID, BROWSER_EXTENDED, BROWSER_GRID, BROWSER_LIST};
    int sizes[] = {ICON_SMALL, ICON_SMALL, ICON_LARGE, ICON_SMALL, ICON_LARGE,
                   ICON_LARGE, ICON_LARGE, ICON_LARGE, ICON_SMALL};
    int columns[] = {4, 4, 3, 4, 2, 4, 4, 3, 4};
    int padding[] = {0, 2, 6, 0, 12, 0, 4, 4, 2};
    for (int theme = 0; theme < 9; theme++) {
        app_init(&app, argv[1], true, true);
        ThemePackage previous = app.themes.active;
        int previous_view = app.browser_style, previous_size = app.icon_size;
        choose(ids[theme]);
        for (int icon = 0; icon < THEME_ICON_COUNT; icon++)
            assert(themes_icon(&app.themes.candidate, icon));
        draw();
        themes_cancel(&app);
        assert(!memcmp(&previous, &app.themes.active, sizeof previous));
        assert(app.browser_style == previous_view && app.icon_size == previous_size);
        choose(ids[theme]);
        themes_apply(&app);
        assert(!strcmp(app.theme_id, ids[theme]) && app.browser_style == views[theme]);
        assert(app.icon_size == sizes[theme]);
        assert(app.themes.active.external_font == (theme != 0 && theme != 3));
        if (theme == 1 || theme == 4 || theme >= 6)
            assert(app.themes.active.font.glyph_count == FONT_GLYPHS);
        assert(app.themes.active.row_padding == padding[theme]);
        if (sizes[theme] == ICON_LARGE && views[theme] == BROWSER_GRID)
            assert(layout_grid_columns(&app) == columns[theme]);
        for (int screen = 0; screen < 2; screen++) {
            themes_background_draw(&app.themes.active, screen, top.pixels);
            bool varied = false;
            for (int pixel = 1; pixel < 256 * 192; pixel++)
                varied |= top.pixels[pixel] != top.pixels[0];
            assert(varied == (*app.themes.active.backgrounds[screen] != 0));
        }
        ThemePalette original_palette = app.themes.active.palette;
        uint16_t resolved[THEME_ACCENT_COUNT];
        for (int accent = 0; accent < THEME_ACCENT_COUNT; accent++) {
            app.accent_index = accent;
            themes_accent_override(&app);
            assert(strlen(themes_accent_name(accent)) <= 6);
            char value[16];
            app.advanced_settings = true;
            settings_value(&app, settings_option(&app, 0, 0), value, sizeof value);
            assert(!strcmp(value, themes_accent_name(accent)));
            ThemePalette palette = app.themes.active.palette;
            assert(abs(luminance(palette.accent) - luminance(palette.bg)) >= 8);
            assert(abs(luminance(palette.ink) - luminance(palette.accent)) >= 8);
            resolved[accent] = palette.accent;
            for (int earlier = 0; earlier < accent; earlier++)
                assert(resolved[accent] != resolved[earlier]);
        }
        assert(app_save(&app));
        app_init(&restored, argv[1], true, true);
        assert(restored.accent_index == THEME_ACCENT_COUNT - 1);
        assert(restored.themes.active.palette.accent == resolved[THEME_ACCENT_COUNT - 1]);
        assert(appearance_save(&app, 2));
        app.accent_index = 0;
        themes_accent_override(&app);
        assert(appearance_load(&app, 2));
        assert(app.accent_index == THEME_ACCENT_COUNT - 1);
        app.accent_index = 0;
        app.theme_accent_override = false;
        app.themes.active.palette = original_palette;
        assert(app_save(&app));
        app_init(&restored, argv[1], true, true);
        assert(!strcmp(restored.theme_id, ids[theme]) && restored.browser_style == views[theme]);
        app.audio.mode = MUSIC_THEME;
        app.audio.effects_enabled = true;
        audio_reload(&app);
        assert((app.audio.music.samples != NULL) == (theme != 0 && theme != 3));
        if (theme != 0 && theme != 3)
            assert(app.audio.music.bytes == 256000 && !*app.audio.status);
        for (int sound = 0; sound < 3; sound++) {
            assert(app.audio.effects[sound].samples && app.audio.effects[sound].bytes);
            audio_free_clip(&app.audio.effects[sound]);
        }
        audio_free_clip(&app.audio.music);
        /* Artwork, its frame and the placeholder card must stay out of the
         * icon dock. Changing only the preview must leave every dock pixel intact. */
        app_category(&app, CATEGORY_GAMES);
        app.themes.active.top_layout = 2;
        app.themes.active.tab_style = 1;
        app.detail_tab = 0;
        app.show_covers = true;
        Artwork previous_artwork = app.artwork;
        app.artwork.ready = false;
        draw();
        memcpy(stationary_rows, top.pixels, sizeof stationary_rows);
        app.artwork.ready = true;
        app.artwork.width = 96;
        for (unsigned pixel = 0; pixel < 96 * 104; pixel++)
            app.artwork.pixels[pixel] = 0x83e0;
        for (int height = 16; height <= 104; height += 88) {
            app.artwork.height = height;
            draw();
            assert(!memcmp(stationary_rows + 144 * 256, top.pixels + 144 * 256,
                           32 * 256 * sizeof *top.pixels));
        }
        app.artwork = previous_artwork;
        /* Shared geometry must still fit with every personal view/size override. */
        for (int category = 0; category <= CATEGORY_SYSTEM; category++) {
            app_category(&app, category);
            for (int view = 0; view < BROWSER_STYLE_COUNT; view++)
                for (int size = 0; size < 2; size++) {
                    app.browser_style = view;
                    app.icon_size = size;
                    browser_reflow(&app);
                    if (!layout_is_grid(&app)) {
                        int text_height = view == BROWSER_EXTENDED ? 26 : 12;
                        assert(layout_row_height(&app) >= text_height + 8);
                        if (layout_has_icons(&app))
                            assert(layout_row_height(&app) >= layout_icon_size(&app) + 8);
                        draw();
                        memcpy(stationary_rows, bottom.pixels, sizeof stationary_rows);
                        int previous_position = app.selection_y_fixed;
                        app.selection_y_fixed += layout_row_height(&app) * 128;
                        draw();
                        assert(!memcmp(stationary_rows, bottom.pixels, sizeof stationary_rows));
                        app.selection_y_fixed = previous_position;
                    }
                    for (int frame = 0; frame < 16; frame++)
                        app_tick(&app);
                    for (unsigned composition = 0; composition < 4; composition++) {
                        app.themes.active.top_layout = composition;
                        app.themes.active.scene_cover_x = composition == 3 ? 158 : 14;
                        app.themes.active.scene_cover_y = composition == 3 ? 26 : 34;
                        app.themes.active.scene_cover_width = composition == 3 ? 96 : 48;
                        app.themes.active.scene_cover_height = composition == 3 ? 90 : 72;
                        for (int detail = 0; detail < 2; detail++) {
                            app.detail_tab = detail;
                            draw();
                        }
                        app.rail_focus = true;
                        app.rail_selected = app.tab_count - 1;
                        draw();
                        app.rail_focus = false;
                    }
                }
        }
        /* Exercise every tab presentation with long custom labels and both ends
         * of a full tab collection; renderers must never change selection. */
        int saved_count = app.tab_count;
        app.tab_count = TAB_CAP;
        for (int i = saved_count; i < TAB_CAP; i++)
            snprintf(app.tabs[i].label, sizeof app.tabs[i].label, "Long custom library label %02d",
                     i);
        app.rail_focus = true;
        app.detail_tab = 0;
        app.browser_style = BROWSER_CLASSIC;
        for (unsigned composition = 0; composition < 4; composition++)
            for (unsigned style = 0; style < 4; style++)
                for (unsigned highlight = 0; highlight < 3; highlight++)
                    for (unsigned visible = 3; visible <= 5; visible++) {
                        app.themes.active.top_layout = composition;
                        app.themes.active.tab_style = style;
                        app.themes.active.tab_highlight = highlight;
                        app.themes.active.tab_visible = visible;
                        app.themes.active.tab_gap = 8;
                        for (int focus = 0; focus < TAB_CAP; focus += TAB_CAP - 1) {
                            app.rail_selected = focus;
                            draw();
                            assert(app.rail_selected == focus && app.tab_count == TAB_CAP);
                        }
                    }
        /* Tile captions use the sidebar text inset, including animated long
         * names. No caption pixel may escape into the frame or main workspace. */
        app.themes.active.tab_style = 1;
        app.rail_selected = TAB_CAP - 1;
        for (unsigned side = 0; side < 2; side++) {
            app.themes.active.top_layout = side;
            int caption_left = side ? 10 : 188;
            for (int motion = 0; motion < 2; motion++) {
                app.reduced_motion = motion;
                for (unsigned frame = 0; frame < 900; frame += 120) {
                    app.frame_count = app.tab_text_started_frame + frame;
                    app.tabs[TAB_CAP - 1].label[0] = 0;
                    draw();
                    memcpy(stationary_rows, top.pixels, sizeof stationary_rows);
                    strcpy(app.tabs[TAB_CAP - 1].label, "A very long custom library name");
                    draw();
                    for (int y = 144; y < 156; y++)
                        for (int x = 0; x < 256; x++)
                            if (x < caption_left || x >= caption_left + 56)
                                assert(top.pixels[y * 256 + x] == stationary_rows[y * 256 + x]);
                }
            }
        }
        app.reduced_motion = false;
        app.tab_count = saved_count;
        app.rail_focus = false;
        for (int level = -1; level <= 16; level++) {
            app.battery_level = level;
            app.battery_charging = false;
            draw();
            app.battery_charging = true;
            draw();
        }
        app.advanced_settings = true;
        settings_open(&app);
        for (int section = 0; section < SETTINGS_SECTION_COUNT; section++) {
            draw();
            settings_section(&app, 1);
        }
        settings_close(&app);
    }
    puts("PASS: shipped themes preview/cancel/apply/restart, Latin-1 font, PNG/JPEG wallpapers, "
         "audio and all browser sizes/views/settings framebuffer bounds");
    return 0;
}
