#include "app.h"
#include "artwork.h"
#include "browser.h"
#include "navigation.h"
#include "paths.h"
#include "preferences.h"
#include "renderer.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static App app, restored, dashboard;
static void put(const char *root, const char *name, const char *text)
{
    char p[PATH_CAP];
    snprintf(p, sizeof p, "%s/%s", root, name);
    FILE *f = fopen(p, "w");
    assert(f);
    fputs(text, f);
    fclose(f);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    app_init(&app, root, true, true);
    assert(app.category == 0 && app.entry_count >= 5);
    assert(app.scroll_speed == SCROLL_NORMAL);
    app_open(&app);
    assert(app.category == 1);
    /* Directory ordering, actual browsing, and category-local position. */
    assert(app.entry_count >= 2 && app.entries[0].kind == ITEM_DIRECTORY);
    app_move(&app, 1);
    int selected = app.locations[1].selected;
    app_category(&app, 3);
    app_category(&app, 1);
    assert(app.locations[1].selected == selected);
    app_panel(&app, PANEL_QUICK);
    /* Horizontal adjustment wraps values, preserves focus and defers saving. */
    app.accent_index = 0;
    app.preferences_dirty = false;
    app.preferences_idle_frames = 50;
    app_panel_adjust(&app, -1);
    assert(app.accent_index == THEME_ACCENT_COUNT - 1 && app.preferences_dirty);
    assert(app.preferences_idle_frames == 0 && app.panel_selection == 0);
    app_panel_adjust(&app, 1);
    assert(app.accent_index == 0);
    app_panel_activate(&app);
    assert(app.accent_index == 1);
    app_panel_move(&app, 1);
    app.reduced_motion = false;
    app_panel_adjust(&app, -1);
    assert(app.reduced_motion);
    app_panel_adjust(&app, 1);
    assert(!app.reduced_motion);
    app_panel_activate(&app);
    assert(app.reduced_motion);
    app_panel_move(&app, 1);
    assert(app.scroll_speed == SCROLL_NORMAL);
    app_panel_adjust(&app, -1);
    assert(app.scroll_speed == SCROLL_SLOW);
    app_panel_adjust(&app, -1);
    assert(app.scroll_speed == SCROLL_FAST);
    app_panel_adjust(&app, 1);
    assert(app.scroll_speed == SCROLL_SLOW);
    app_panel_activate(&app);
    assert(app.scroll_speed == SCROLL_NORMAL);
    assert(navigation_scroll_timing(SCROLL_SLOW).interval_frames >
           navigation_scroll_timing(SCROLL_NORMAL).interval_frames);
    assert(navigation_scroll_timing(SCROLL_NORMAL).interval_frames >
           navigation_scroll_timing(SCROLL_FAST).interval_frames);
    assert(navigation_scroll_timing(SCROLL_FAST).delay_frames <
           navigation_scroll_timing(SCROLL_SLOW).delay_frames);
    /* Left/right must not invoke navigation, system-return or save actions. */
    app.preferences_dirty = false;
    for (int row = QUICK_HOME; row < QUICK_CONTROL_COUNT; row++) {
        app.panel_selection = row;
        app_panel_adjust(&app, 1);
        app_panel_adjust(&app, -1);
        assert(app.panel == PANEL_QUICK && app.category == 1);
        assert(app.panel_selection == row && !app.preferences_dirty);
    }
    app_back(&app);
    app_panel_adjust(&app, 1);
    assert(app.accent_index == 1 && app.panel == PANEL_NONE);
    app_panel(&app, PANEL_QUICK);
    app_panel_move(&app, 1);
    app_back(&app);
    assert(app.panel == PANEL_NONE && app.locations[1].selected == selected);
    app_toggle_favorite(&app);
    assert(app_is_favorite(&app, app_selected(&app)->path));
    app.accent_index = 2;
    app.reduced_motion = true;
    app.scroll_speed = SCROLL_FAST;
    app_save(&app);
    assert(!app.preferences_dirty);
    app_init(&restored, root, true, true);
    assert(restored.scroll_speed == SCROLL_FAST);
    assert(restored.accent_index == 2 && restored.reduced_motion && restored.favorite_count == 1);
    app_category(&app, 4);
    app_open(&app);
    assert(app.system_discovery_requested && app.system_title_requested == 0);
    app.system_title_requested = -1;
    app.system_catalog.attempted = true;
    app_open(&app);
    assert(app.panel == PANEL_MESSAGE && strstr(app.message, "not discovered"));
    /* Corrupt state must not load a directory outside the configured test SD. */
    put(root, "_nds/dsi-frontend/state.ini", "version=1\ncategory=1\nlocation=1,0,0,/etc\n");
    app_init(&restored, root, true, true);
    assert(strstr(restored.locations[1].path, root) == restored.locations[1].path);
    app_category(&app, 5);
    app_category(&app, 1);
    app.locations[1].selected = 0;
    app_scan(&app);
    app_open(&app);
    assert(strstr(app.locations[1].path, "Folder"));
    app_back(&app);
    assert(app.category == 1);
    /* PNG decoding is lazy, bounded and content-type independent. */
    app.locations[1].selected = 1;
    artwork_load(&app);
    assert(app.artwork.ready && app.artwork.width > 0 && app.artwork.width <= 96);
    app_reset_preview(&app);
    app.preferences_dirty = false;
    for (int i = 0; i < 6; i++)
        app_tick(&app);
    assert(!app.artwork.ready);
    app_tick(&app);
    assert(app.artwork.ready);
    /* Opening settings while the lazy-load delay expires must not permanently
     * suppress the selected cover after settings closes. */
    app_reset_preview(&app);
    app.settings.open = true;
    for (int i = 0; i < 9; i++)
        app_tick(&app);
    assert(!app.artwork.ready);
    app.settings.open = false;
    app_tick(&app);
    assert(app.artwork.ready);
    /* Fresh tab defaults match the chosen SD layout; saved paths remain separate. */
    char expected[PATH_CAP];
    path_default_category(expected, root, CATEGORY_GAMES);
    char games_path[PATH_CAP];
    assert(path_join(games_path, root, "roms"));
    assert(strcmp(expected, games_path) == 0);
    path_default_category(expected, root, CATEGORY_APPS);
    char apps_path[PATH_CAP];
    assert(path_join(apps_path, root, "apps"));
    assert(strcmp(expected, apps_path) == 0);
    path_default_category(expected, root, CATEGORY_MEDIA);
    char media_path[PATH_CAP];
    assert(path_join(media_path, root, "media"));
    assert(strcmp(expected, media_path) == 0);
    snprintf(expected, sizeof expected, "%s/../elsewhere", root);
    assert(!path_is_within_root(expected, root));
    snprintf(expected, sizeof expected, "%s-other/roms", root);
    assert(!path_is_within_root(expected, root));

    put(root, "_nds/dsi-frontend/state.ini", "version=1\nscroll_speed=99\n");
    app_init(&restored, root, true, true);
    assert(restored.scroll_speed == SCROLL_NORMAL);
    put(root, "_nds/dsi-frontend/state.ini", "version=1\nscroll_speed=-1\n");
    app_init(&restored, root, true, true);
    assert(restored.scroll_speed == SCROLL_NORMAL);

    char old_apps_state[PATH_CAP + 80];
    snprintf(old_apps_state, sizeof old_apps_state,
             "version=1\nlocation=2,4,2,%s/roms/DS(i) Homebrew\n", root);
    put(root, "_nds/dsi-frontend/state.ini", old_apps_state);
    app_init(&restored, root, true, true);
    assert(strcmp(restored.locations[CATEGORY_APPS].path, apps_path) == 0);
    assert(restored.locations[CATEGORY_APPS].selected == 0 && restored.preferences_dirty);
    snprintf(old_apps_state, sizeof old_apps_state, "version=1\nlocation=2,0,0,%s/custom-apps\n",
             root);
    put(root, "_nds/dsi-frontend/state.ini", old_apps_state);
    app_init(&restored, root, true, true);
    assert(strstr(restored.locations[CATEGORY_APPS].path, "/custom-apps"));

    /* The original scratch's state is read, then saved under _nds without deleting it. */
    char current[PATH_CAP], backup[PATH_CAP], legacy[PATH_CAP];
    path_preferences_file(current, root, "state.ini");
    path_preferences_file(backup, root, "state.bak");
    remove(current);
    remove(backup);
    path_join(legacy, root, "data");
    mkdir(legacy, 0777);
    path_join(legacy, root, FRONTEND_LEGACY_DIRECTORY);
    mkdir(legacy, 0777);
    put(root, FRONTEND_LEGACY_DIRECTORY "/state.ini", "version=1\ntheme=1\ncategory=0\n");
    app_init(&restored, root, true, true);
    assert(restored.accent_index == 1 && restored.preferences_dirty);
    assert(app_save(&restored));
    FILE *saved = fopen(current, "r");
    assert(saved);
    fclose(saved);
    path_join(legacy, root, FRONTEND_LEGACY_DIRECTORY "/state.ini");
    saved = fopen(legacy, "r");
    assert(saved);
    fclose(saved);

    /* Text-only Home uses the reclaimed row; scrolling starts at its tenth item. */
    app_init(&dashboard, root, true, true);
    dashboard.browser_style = BROWSER_LIST;
    app_category(&dashboard, CATEGORY_HOME);
    for (int index = 0; index < 6; index++) {
        char relative[64];
        snprintf(relative, sizeof relative, "roms/Library %d.txt", index);
        put(root, relative, "Dashboard row fixture.\n");
        path_join(dashboard.favorites[dashboard.favorite_count++], root, relative);
    }
    app_scan(&dashboard);
    app_move(&dashboard, 8);
    assert(dashboard.locations[CATEGORY_HOME].selected == 8);
    assert(dashboard.locations[CATEGORY_HOME].scroll == 0);
    app_move(&dashboard, 1);
    assert(dashboard.locations[CATEGORY_HOME].selected == 9);
    assert(dashboard.locations[CATEGORY_HOME].scroll == 1);

    puts("PASS: browser, category state, overlays, favorites, persistence, path isolation, lazy "
         "universal PNG artwork");
    return 0;
}
