#include "app.h"
#include "artwork.h"
#include "associations.h"
#include "browser.h"
#include "grid.h"
#include "icons.h"
#include "layout.h"
#include "paths.h"
#include "preferences.h"
#include "renderer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app, restored;
static struct {
    uint32_t before;
    uint16_t pixels[256 * 192];
    uint32_t after;
} top, bottom;

static void directory(const char *root, const char *name)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    assert(mkdir(path, 0777) == 0);
}
static void text_file(const char *root, const char *name, const char *contents)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fputs(contents, file) >= 0);
    assert(fclose(file) == 0);
}
/* Synthetic metadata fixture, not a playable ROM or copied commercial asset. */
static void banner_file(const char *root, const char *name, uint16_t color)
{
    unsigned char header[0x200] = {0}, banner[0x840] = {0};
    header[0x69] = 2;
    banner[0] = 3;
    banner[1] = 1;
    memset(banner + 0x20, 0x11, 512);
    banner[0x20] = 0x10;
    banner[0x20 + 32] = 0x22;
    banner[0x222] = color & 255;
    banner[0x223] = color >> 8;
    banner[0x224] = 0xe0;
    banner[0x225] = 3;
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(header, 1, sizeof header, file) == sizeof header);
    assert(fwrite(banner, 1, sizeof banner, file) == sizeof banner);
    assert(fclose(file) == 0);
}
static int find_entry(const char *name)
{
    for (int index = 0; index < app.entry_count; index++)
        if (!strcmp(app.entries[index].name, name))
            return index;
    assert(!"missing test entry");
    return -1;
}
static void settle(void)
{
    for (int frame = 0; frame < 24; frame++) {
        app.frame_count++;
        icons_tick(&app);
    }
}
static void select_entry(const char *name)
{
    app_move(&app, find_entry(name) - app.locations[app.category].selected);
    settle();
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    directory(root, "apps");
    directory(root, "roms");
    directory(root, "_nds");
    directory(root, "_nds/dsi-frontend");
    directory(root, "_nds/dsi-frontend/plugins");
    banner_file(root, "apps/My Emulator.nds", 31);
    banner_file(root, "apps/Other.nds", 31 << 10);
    banner_file(root, "roms/Native.dsi", 31 << 10);
    banner_file(root, "roms/Native.srl", 31 << 10);
    banner_file(root, "roms/Native.app", 31 << 10);
    text_file(root, "roms/Bad.nds", "truncated banner");
    text_file(root, "roms/Game.SFC", "association fixture, not a game");
    text_file(root, "roms/Second.smc", "association fixture, not a game");
    text_file(root, "roms/Shortcut.argv",
              "# comment\n\"sd:/apps/My Emulator.nds\" \"sd:/roms/Game.SFC\"\n");
    text_file(root, "roms/Bad.argv", "\"sd:/apps/unterminated.nds\n");
    text_file(root, "_nds/dsi-frontend/plugins/10-snes.ini",
              "[plugin]\nversion=1\nextensions=sfc, .smc\napplication=sd:/apps/Other.nds\n"
              "dsipath=sd:/apps/My Emulator.nds\nntrpath=sd:/apps/Other.nds\n"
              "content_root=sd:/roms\n");
    text_file(root, "_nds/dsi-frontend/plugins/20-snes.ini",
              "[plugin]\nextensions=sfc\napplication=sd:/apps/Other.nds\n"
              "content_root=sd:/another-folder\n");
    text_file(root, "_nds/dsi-frontend/plugins/escape.ini",
              "[plugin]\nextensions=sfc\napplication=../outside.nds\n");
    text_file(root, "_nds/dsi-frontend/plugins/version.ini",
              "[plugin]\nversion=999\nextensions=sfc\napplication=sd:/apps/Other.nds\n");
    char oversized[5000];
    memset(oversized, 'x', sizeof oversized - 1);
    oversized[sizeof oversized - 1] = 0;
    text_file(root, "_nds/dsi-frontend/plugins/oversized.ini", oversized);
    text_file(root, "roms/Oversized.argv", oversized);
    app_init(&app, root, true, true);
    assert(app.association_count == 2);
    app_category(&app, CATEGORY_GAMES);
    assert(app.icon_reads == 0); /* No icon I/O during scanning. */
    select_entry("Game.SFC");
    const BrowserIcon *icon = icons_for_entry(&app, find_entry("Game.SFC"));
    assert(icon && icon->pixels[0] == 0 && icon->pixels[1] == 0x801f);
    assert(icon->pixels[8] == 0x83e0); /* Tile order and palette decoding. */
    select_entry("Second.smc");
    assert(icons_for_entry(&app, find_entry("Second.smc")) == icon);
    app.artwork.ready = true;
    app.artwork.width = app.artwork.height = 32;
    for (int pixel = 0; pixel < 32 * 32; pixel++)
        app.artwork.pixels[pixel] = 0xffff;
    assert(icons_for_entry(&app, find_entry("Second.smc"))->pixels[1] == 0x801f);
    unsigned reads = app.icon_reads;
    settle();
    assert(app.icon_reads == reads);
    select_entry("Shortcut.argv");
    assert(icons_for_entry(&app, find_entry("Shortcut.argv")) == icon);
    select_entry("Native.dsi");
    assert(icons_for_entry(&app, find_entry("Native.dsi"))->pixels[1] == (0x8000 | (31 << 10)));
    select_entry("Native.app");
    assert(icons_for_entry(&app, find_entry("Native.app")));
    select_entry("Native.srl");
    assert(icons_for_entry(&app, find_entry("Native.srl")));
    select_entry("Oversized.argv");
    assert(!icons_for_entry(&app, find_entry("Oversized.argv")));
    select_entry("Bad.nds");
    assert(!icons_for_entry(&app, find_entry("Bad.nds")));
    select_entry("Bad.argv");
    assert(!icons_for_entry(&app, find_entry("Bad.argv")));
    /* Changing layout retains selection and keeps the selected row visible. */
    select_entry("Second.smc");
    int selected = app.locations[CATEGORY_GAMES].selected;
    app_panel(&app, PANEL_QUICK);
    for (int size = 0; size < ICON_SIZE_COUNT; size++) {
        for (int style = 0; style < BROWSER_STYLE_COUNT; style++) {
            app.icon_size = size;
            app.browser_style = style;
            browser_reflow(&app);
            assert(app.locations[CATEGORY_GAMES].selected == selected);
            assert(selected >= app.locations[CATEGORY_GAMES].scroll);
            assert(selected < app.locations[CATEGORY_GAMES].scroll + layout_visible_rows(&app));
            assert(layout_list_bottom(&app) <= UI_LIST_BOTTOM);
            assert((layout_list_bottom(&app) - layout_list_top(app.category)) %
                       layout_row_height(&app) ==
                   0);
            top.before = top.after = bottom.before = bottom.after = 0x12345678;
            app.panel = PANEL_NONE;
            render_app(&app, top.pixels, bottom.pixels, "12:00");
            assert(top.before == 0x12345678 && top.after == 0x12345678);
            assert(bottom.before == 0x12345678 && bottom.after == 0x12345678);
        }
    }
    /* Home and System use the same appearance and hit targets as folder tabs. */
    const int categories[] = {CATEGORY_HOME, CATEGORY_SYSTEM};
    for (unsigned c = 0; c < sizeof categories / sizeof categories[0]; c++) {
        int category = categories[c];
        app_category(&app, category);
        assert(app.entry_count > 0);
        for (int size = 0; size < ICON_SIZE_COUNT; size++) {
            for (int style = 0; style < BROWSER_STYLE_COUNT; style++) {
                app.icon_size = size;
                app.browser_style = style;
                browser_reflow(&app);
                assert(layout_icon_size(&app) == (size == ICON_LARGE ? 32 : 16));
                assert(layout_is_grid(&app) == (style == BROWSER_GRID));
                assert(layout_row_height(&app) >= layout_icon_size(&app) || style == BROWSER_LIST);
                app_move(&app, app.entry_count - 1);
                browser_reflow(&app);
                Location *location = &app.locations[category];
                int cell = location->selected - location->scroll;
                assert(cell >= 0 && cell < layout_visible_rows(&app));
                if (layout_is_grid(&app)) {
                    int x = 6 + cell % layout_grid_columns(&app) * layout_grid_cell_width(&app) + 8;
                    int y = layout_list_top(category) +
                            cell / layout_grid_columns(&app) * layout_row_height(&app) + 8;
                    assert(grid_touch_index(&app, x, y) == location->selected);
                }
                top.before = top.after = bottom.before = bottom.after = 0x12345678;
                render_app(&app, top.pixels, bottom.pixels, "12:00");
                assert(top.before == 0x12345678 && top.after == 0x12345678);
                assert(bottom.before == 0x12345678 && bottom.after == 0x12345678);
            }
        }
    }
    app_category(&app, CATEGORY_GAMES);
    /* List reclaims the icon column and ignores remembered icon size. */
    app.browser_style = BROWSER_LIST;
    app.icon_size = ICON_LARGE;
    browser_reflow(&app);
    assert(!layout_has_icons(&app) && layout_row_height(&app) == 16);
    assert(layout_visible_rows(&app) == 8);
    icons_reset(&app);
    reads = app.icon_reads;
    settle();
    assert(app.icon_reads == reads);
    render_app(&app, top.pixels, bottom.pixels, "12:00");
    for (int pixel = 0; pixel < 256 * 192; pixel++)
        assert(bottom.pixels[pixel] != 0x801f); /* The red banner is absent below. */
    app_panel(&app, PANEL_QUICK);
    app.panel_selection = QUICK_ICON_SIZE;
    app.preferences_dirty = false;
    app_panel_adjust(&app, 1);
    assert(app.icon_size == ICON_LARGE && !app.preferences_dirty);
    app.panel_selection = QUICK_BROWSER_STYLE;
    app_panel_adjust(&app, -1);
    assert(app.browser_style == BROWSER_EXTENDED);
    app_panel_adjust(&app, -1);
    assert(app.browser_style == BROWSER_CLASSIC);
    app_panel_adjust(&app, -1);
    assert(app.browser_style == BROWSER_GRID);
    app_panel_adjust(&app, 1);
    assert(app.browser_style == BROWSER_CLASSIC);
    app.panel_selection = QUICK_ICON_SIZE;
    app_panel_adjust(&app, 1);
    assert(app.icon_size == ICON_SMALL);
    app.panel_selection = QUICK_BROWSER_STYLE;
    app_panel_move(&app, 4);
    assert(app.panel_selection == QUICK_SAVE && app.panel_scroll == 2);
    app_panel_move(&app, 1);
    assert(app.panel_selection == QUICK_SETTINGS && app.panel_scroll == 3);
    app_panel_move(&app, 1);
    assert(app.panel_selection == QUICK_ACCENT && app.panel_scroll == 0);
    app.icon_size = ICON_LARGE;
    app.browser_style = BROWSER_LIST;
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(restored.icon_size == ICON_LARGE && restored.browser_style == BROWSER_LIST);
    text_file(root, "_nds/dsi-frontend/state.ini", "version=1\nbrowser_style=1\n");
    app_init(&restored, root, true, true);
    assert(restored.browser_style == BROWSER_EXTENDED);
    text_file(root, "_nds/dsi-frontend/state.ini", "version=1\nicon_size=99\nbrowser_style=-1\n");
    app_init(&restored, root, true, false);
    assert(restored.icon_size == ICON_SMALL && restored.browser_style == BROWSER_CLASSIC);
    char resolved[PATH_CAP], expected[PATH_CAP];
    assert(path_join(expected, root, "apps/Other.nds"));
    assert(association_icon_application(&restored, "Game.sfc", resolved));
    assert(!strcmp(expected, resolved));
    /* Exercise cache eviction across more native files than cache slots. */
    for (int i = 0; i < ICON_CACHE_CAP + 4; i++) {
        char name[64];
        snprintf(name, sizeof name, "roms/Title %02d.nds", i);
        banner_file(root, name, (uint16_t)(i + 1));
    }
    app.panel = PANEL_NONE;
    app.browser_style = BROWSER_CLASSIC;
    app_scan(&app);
    for (int i = 0; i < ICON_CACHE_CAP + 4; i++) {
        char name[64];
        snprintf(name, sizeof name, "Title %02d.nds", i);
        select_entry(name);
        assert(icons_for_entry(&app, find_entry(name))->pixels[1] == (0x8000 | (i + 1)));
    }
    /* System actions have original fallbacks even without NAND or SD access. */
    app_category(&app, CATEGORY_SYSTEM);
    app.browser_style = BROWSER_CLASSIC;
    app.icon_size = ICON_SMALL;
    app.locations[app.category].selected = app.locations[app.category].scroll = 0;
    app.storage_ready = false;
    icons_reset(&app);
    reads = app.icon_reads;
    settle();
    assert(app.icon_reads == reads);
    uint16_t fallback[7][32 * 32];
    for (int i = 0; i < 7; i++) {
        assert(icon_system_fallback(i, fallback[i]));
        assert(icons_for_entry(&app, i));
        assert(!memcmp(icons_for_entry(&app, i)->pixels, fallback[i], sizeof fallback[i]));
        assert(fallback[i][0] == 0); /* Transparent margins. */
        for (int j = 0; j < i; j++)
            assert(memcmp(fallback[i], fallback[j], sizeof fallback[i]));
    }
    assert(!icon_system_fallback(-1, top.pixels));
    assert(!icon_system_fallback(7, top.pixels));
    app.storage_ready = true;
    app.browser_style = BROWSER_LIST;
    app.locations[app.category].selected = 1;
    app.system_catalog.titles[1].available = true;
    assert(path_join(app.system_catalog.content_paths[1], root, "apps/My Emulator.nds"));
    icons_reset(&app);
    settle();
    const BrowserIcon *camera = icons_for_entry(&app, 1);
    assert(camera && camera->pixels[1] == 0x801f); /* Native banner wins. */
    assert(!icons_for_entry(&app, 0));             /* List loads only the top-screen preview. */
    app.show_covers = true;
    render_app(&app, top.pixels, bottom.pixels, "12:00");
    assert(top.pixels[73 * 256 + 33] == 0x801f);
    for (int pixel = 0; pixel < 256 * 192; pixel++)
        assert(bottom.pixels[pixel] != 0x801f); /* List remains text-only. */
    assert(path_join(app.system_catalog.content_paths[1], root, "roms/Bad.nds"));
    icons_reset(&app);
    settle();
    camera = icons_for_entry(&app, 1);
    assert(camera && !memcmp(camera->pixels, fallback[1], sizeof fallback[1]));
    reads = app.icon_reads;
    settle();
    assert(app.icon_reads == reads); /* A missing/corrupt banner is not retried every frame. */
    puts("PASS: native icons, associations, argv metadata, cache reuse/eviction, layouts and "
         "persistence");
    return 0;
}
