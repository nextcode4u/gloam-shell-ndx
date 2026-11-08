#include "app.h"
#include "browser.h"
#include "paths.h"
#include "preferences.h"
#include "renderer.h"
#include "tabs.h"
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

static void write_file(const char *root, const char *name, const char *contents)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fputs(contents, file) >= 0);
    assert(fclose(file) == 0);
}
static void make_directory(const char *root, const char *name)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    assert(mkdir(path, 0777) == 0);
}
static void check_rail(void)
{
    assert(app.tab_scroll >= 0);
    assert(app.category >= app.tab_scroll);
    assert(app.category < app.tab_scroll + TAB_VISIBLE_COUNT);
    assert(app.tab_scroll <=
           (app.tab_count > TAB_VISIBLE_COUNT ? app.tab_count - TAB_VISIBLE_COUNT : 0));
    top.before = top.after = bottom.before = bottom.after = 0x12345678;
    render_app(&app, top.pixels, bottom.pixels, "12:00");
    assert(top.before == 0x12345678 && top.after == 0x12345678);
    assert(bottom.before == 0x12345678 && bottom.after == 0x12345678);
    for (int y = 36; y < 152; y++)
        for (int x = 248; x < 256; x++)
            assert(top.pixels[y * 256 + x] == top.pixels[0]);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    make_directory(root, "_nds");
    make_directory(root, "_nds/dsi-frontend");
    make_directory(root, "roms");
    make_directory(root, "roms/A");
    make_directory(root, "roms/A/Sub");
    make_directory(root, "roms/B");
    write_file(root, "roms/A/Sub/one.txt", "one");
    write_file(root, "roms/A/Sub/two.txt", "two");
    app_init(&app, root, true, true);
    assert(app.tab_count == CATEGORY_COUNT && app.tab_config_skipped == 0);
    /* Built-in libraries stop at their defaults, including absent folders. */
    for (int category = CATEGORY_GAMES; category <= CATEGORY_MEDIA; category++) {
        app_category(&app, category);
        Location before = app.locations[category];
        app_back(&app);
        assert(app.category == category);
        assert(!memcmp(&before, &app.locations[category], sizeof before));
    }
    const char *config = "_nds/dsi-frontend/tabs.ini";
    write_file(root, config,
               "[tabs]\nversion=1\n[tab.snes]\nlabel=Super Nintendo\npath=sd:/roms/A\n"
               "[tab.music]\nlabel=Music\npath=/media/music\n"
               "[tab.work]\nlabel=A long custom tab label with spaces\nfolder=roms/B\n");
    app_init(&app, root, true, true);
    assert(app.tab_count == 9 && !app.tab_config_skipped);
    assert(tabs_find(&app, "SNES") == 6 && tabs_find(&app, "work") == 8);
    assert(!strcmp(tabs_label(&app, 8), "A long custom tab label with spaces"));
    /* L/R-style movement scrolls the window and wraps at both ends. */
    for (int index = 0; index < app.tab_count; index++) {
        app_category(&app, index);
        check_rail();
    }
    assert(app.tab_scroll == 3);
    app_category(&app, app.category + 1);
    assert(app.category == 0 && app.tab_scroll == 0);
    app_category(&app, -1);
    assert(app.category == 8 && app.tab_scroll == 3);
    for (int index = app.tab_count - 1; index >= 0; index--) {
        app_category(&app, index);
        check_rail();
    }
    /* Folder tabs remember nested navigation by ID, not their current index. */
    app_category(&app, tabs_find(&app, "snes"));
    assert(app.entry_count == 1 && app.entries[0].kind == ITEM_DIRECTORY);
    app_open(&app);
    assert(strstr(app.locations[app.category].path, "/roms/A/Sub"));
    app_move(&app, 1);
    assert(app_save(&app));
    write_file(root, config,
               "[tabs]\nversion=1\n[tab.work]\nlabel=Work\npath=roms/B\n"
               "[tab.music]\npath=media/music\n[tab.snes]\nlabel=SNES renamed\npath=roms/A\n");
    app_init(&restored, root, true, true);
    assert(restored.category == 8 && !strcmp(restored.tabs[8].id, "snes"));
    assert(strstr(restored.locations[8].path, "/roms/A/Sub"));
    assert(restored.locations[8].selected == 1 && restored.tab_scroll == 3);
    app_back(&restored);
    assert(strstr(restored.locations[8].path, "/roms/A"));
    app_back(&restored);
    assert(restored.category == 8 && !strcmp(restored.locations[8].path, restored.tabs[8].home));
    /* Moving the assigned root does not resurrect an unrelated old folder. */
    write_file(root, config, "[tabs]\nversion=1\n[tab.snes]\npath=roms/B\n");
    app_init(&restored, root, true, true);
    assert(restored.category == 6 && strstr(restored.locations[6].path, "/roms/B"));
    assert(restored.locations[6].selected == 0);
    write_file(root, config, "[tabs]\nversion=1\n[tab.other]\npath=roms/B\n");
    app_init(&restored, root, true, true);
    assert(restored.category == CATEGORY_HOME); /* Removed active ID. */
    write_file(root, config,
               "[tabs]\nversion=1\n[tab.good]\npath=sd:/roms//A/./\n"
               "[tab.good]\npath=roms/B\n[tab.home]\npath=roms/A\n"
               "[tab.escape]\npath=../outside\n[tab.device]\npath=fat:/roms/A\n"
               "[tab.missing]\npath=roms/Not installed\n");
    app_init(&app, root, true, true);
    assert(app.tab_count == 8 && app.tab_config_skipped == 4);
    assert(app.panel == PANEL_MESSAGE);
    app_category(&app, tabs_find(&app, "missing"));
    assert(app.directory_error && app.entry_count == 0);
    app_back(&app);
    assert(app.category == tabs_find(&app, "missing") && app.directory_error);
    write_file(root, "_nds/dsi-frontend/state.ini",
               "version=1\nactive_tab=good\ntab_location=good,0,0,/etc\n");
    app_init(&restored, root, true, true);
    assert(restored.category == 6 && strstr(restored.locations[6].path, "/roms/A"));
    /* Root overrides reset stale sibling paths but retain nested locations. */
    write_file(root, config,
               "[tabs]\nversion=1\n[roots]\ngames=roms/A\napps=sd:/roms/B\nmedia=/media/music\n"
               "[tab.nested]\npath=roms/A/Sub\n");
    char state[4096];
    snprintf(state, sizeof state,
             "version=1\nactive_tab=games\nlocation=1,4,2,%s/roms/B\n"
             "location=2,0,0,%s/roms/B\nlocation=3,0,0,%s/roms/A\nfolder_batch=games,2\n",
             root, root, root);
    write_file(root, "_nds/dsi-frontend/state.ini", state);
    app_init(&restored, root, true, true);
    assert(restored.category == CATEGORY_GAMES && restored.preferences_dirty);
    assert(restored.locations[CATEGORY_GAMES].batch == 0);
    assert(!strcmp(restored.locations[1].path, restored.tabs[1].home));
    assert(strstr(restored.tabs[1].home, "/roms/A"));
    assert(strstr(restored.tabs[2].home, "/roms/B"));
    assert(strstr(restored.locations[3].path, "/media/music"));
    app_open(&restored);
    assert(strstr(restored.locations[1].path, "/roms/A/Sub"));
    assert(app_save(&restored));
    app_init(&app, root, true, true);
    assert(strstr(app.locations[1].path, "/roms/A/Sub"));
    app_back(&app);
    app_back(&app);
    assert(app.category == CATEGORY_GAMES && !strcmp(app.locations[1].path, app.tabs[1].home));
    /* A forged directory entry and an invalid in-memory location cannot escape. */
    app.entries[0].kind = ITEM_DIRECTORY;
    assert(path_join(app.entries[0].path, root, "roms/B"));
    app.entry_count = 1;
    app.locations[1].selected = 0;
    app_open(&app);
    assert(app.panel == PANEL_MESSAGE && !strcmp(app.locations[1].path, app.tabs[1].home));
    app.panel = PANEL_NONE;
    assert(path_join(app.locations[1].path, root, "roms/A-sibling"));
    app_scan(&app);
    assert(!strcmp(app.locations[1].path, app.tabs[1].home));
    app_category(&app, CATEGORY_FILES);
    assert(!strcmp(app.locations[CATEGORY_FILES].path, root) ||
           strlen(app.locations[CATEGORY_FILES].path) == strlen(root) + 1);
    app_back(&app);
    assert(app.category == CATEGORY_HOME);
    write_file(root, config,
               "[tabs]\nversion=1\n[roots]\ngames=../outside\napps=fat:/apps\nfiles=roms/A\n");
    app_init(&app, root, true, true);
    assert(app.tab_config_skipped == 3);
    char default_root[PATH_CAP];
    path_default_category(default_root, root, CATEGORY_GAMES);
    assert(!strcmp(app.tabs[CATEGORY_GAMES].home, default_root));

    /* Bound excessive configurations without losing any built-in destination. */
    char large[16384] = "[tabs]\nversion=1\n";
    size_t used = strlen(large);
    for (int index = 0; index < TAB_CAP; index++)
        used += (size_t)snprintf(large + used, sizeof large - used,
                                 "[tab.extra%02d]\nlabel=Extra %02d\npath=roms/A\n", index, index);
    write_file(root, config, large);
    app_init(&app, root, true, true);
    assert(app.tab_count == TAB_CAP && app.tab_config_skipped == CATEGORY_COUNT);
    for (int index = 0; index < TAB_CAP; index++) {
        app_category(&app, index);
        check_rail();
    }
    app_category(&app, 2);
    Location origin = app.locations[2];
    int scroll = app.tab_scroll;
    app.frame_count = 123;
    app_rail_toggle(&app);
    assert(app.tab_text_started_frame == 123);
    assert(app.rail_focus && app.rail_selected == 2);
    for (int i = 0; i < 20; i++)
        app_rail_move(&app, 1);
    assert(app.rail_selected == 22 && app.category == 2);
    assert(app.tab_scroll + TAB_VISIBLE_COUNT > app.rail_selected);
    render_app(&app, top.pixels, bottom.pixels, "12:00");
    app_back(&app);
    assert(!app.rail_focus && app.tab_scroll == scroll && app.category == 2);
    assert(!memcmp(&origin, &app.locations[2], sizeof origin));
    app_rail_toggle(&app);
    app_rail_move(&app, -1);
    app_rail_commit(&app);
    assert(app.category == 1 && !app.rail_focus);
    app_toggle_details(&app);
    int detail = app.detail_tab;
    app_category(&app, 2);
    app_toggle_details(&app);
    app_toggle_details(&app);
    app_category(&app, 1);
    assert(app.detail_tab == detail);
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(restored.detail_tab == detail && restored.tab_details[1] == detail);
    write_file(root, config, "[tabs]\nversion=999\n[tab.invalid]\npath=roms/A\n");
    app_init(&app, root, true, true);
    assert(app.tab_count == CATEGORY_COUNT && app.tab_config_skipped > 0);
    char path[PATH_CAP];
    assert(path_preferences_file(path, root, "tabs.ini"));
    FILE *file = fopen(path, "wb");
    assert(file);
    for (int index = 0; index < 65537; index++)
        assert(fputc('x', file) != EOF);
    assert(fclose(file) == 0);
    app_init(&app, root, true, true);
    assert(app.tab_count == CATEGORY_COUNT && app.tab_config_skipped > 0);
    assert(path_resolve_sd(path, root, "sd:/roms//A/./Sub/"));
    assert(strstr(path, "/roms/A/Sub"));
    assert(!path_resolve_sd(path, root, "roms/A/../B"));
    assert(!path_resolve_sd(path, root, "roms\\A"));
    puts("PASS: configured tabs, L/R rail scrolling/wrap, ID persistence, folder roots and invalid "
         "config recovery");
    return 0;
}
