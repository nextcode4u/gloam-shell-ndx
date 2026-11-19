#include "app.h"
#include "appearance.h"
#include "artwork.h"
#include "browser.h"
#include "file_types.h"
#include "game_options.h"
#include "item_preferences.h"
#include "paths.h"
#include "preferences.h"
#include "renderer.h"
#include "settings.h"
#include "themes.h"
#include "top_content.h"
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
static bool count_reads;
static unsigned opens;
FILE *__real_fopen(const char *, const char *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (count_reads)
        opens++;
    return __real_fopen(path, mode);
}
static void write_text(const char *name, const char *value)
{
    char path[PATH_CAP];
    assert(path_join(path, app.root, name));
    FILE *f = fopen(path, "wb");
    assert(f && fputs(value, f) >= 0 && !fclose(f));
}
static void prepare(void)
{
    app_reset_preview(&app);
    app.selection_idle_frames = 7;
    for (int i = 0; i < 4; i++)
        top_content_tick(&app);
    assert(app.top.stage == 4);
}
static void draw(void)
{
    top.before = bottom.before = 0x13579BDF;
    top.after = bottom.after = 0x2468ACE0;
    unsigned reads = opens;
    count_reads = true;
    render_app(&app, top.pixels, bottom.pixels, "22:30");
    count_reads = false;
    assert(opens == reads);
    assert(top.before == 0x13579BDF && bottom.before == 0x13579BDF);
    assert(top.after == 0x2468ACE0 && bottom.after == 0x2468ACE0);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    /* User choices stay small; persisted enum IDs never shift. */
    assert(top_content_step(TOP_CONTENT_THEME, 1) == TOP_CONTENT_NEIGHBORS);
    assert(top_content_step(TOP_CONTENT_NEIGHBORS, 1) == TOP_CONTENT_SMART);
    assert(top_content_step(TOP_CONTENT_SMART, 1) == TOP_CONTENT_THEME);
    assert(top_content_step(TOP_CONTENT_THEME, -1) == TOP_CONTENT_SMART);
    assert(top_content_step(TOP_CONTENT_SMART, -1) == TOP_CONTENT_NEIGHBORS);
    assert(top_content_step(TOP_CONTENT_NEIGHBORS, -1) == TOP_CONTENT_THEME);
    assert(top_content_preference(TOP_CONTENT_CONTEXT) == TOP_CONTENT_SMART);
    assert(top_content_preference(TOP_CONTENT_FOLDER) == TOP_CONTENT_SMART);
    assert(top_content_preference(TOP_CONTENT_ACTIVITY) == TOP_CONTENT_SMART);
    assert(top_content_preference(TOP_CONTENT_ORIGINAL) == TOP_CONTENT_THEME);
    assert(top_content_preference(999) == TOP_CONTENT_THEME);
    app_init(&app, argv[1], true, true);
    assert(app.top_content == TOP_CONTENT_THEME && top_content_mode(&app) == TOP_CONTENT_THEME);
    for (unsigned legacy = 1; legacy < TOP_CONTENT_COUNT; legacy++) {
        char record[64];
        snprintf(record, sizeof record, "version=1\ntop_content=%u\n", legacy);
        write_text("_nds/dsi-frontend/state.ini", record);
        app_init(&restored, argv[1], true, true);
        assert(restored.top_content == top_content_preference(legacy));
    }

    app_category(&app, CATEGORY_GAMES);
    assert(app.entry_count == 0);
    write_text("roms/Fixture.txt", "Authored metadata fixture, not a game");
    write_text("roms/Fixture.txt.gloam.ini",
               "version=1\ntitle=星の図書館\ndeveloper=Example "
               "author\ngenre=Adventure\nyear=2026\ndescription=A long description that stays "
               "readable as a bounded continuous scrolling line.\n");
    browser_refresh(&app);
    assert(app.entry_count == 1); /* Even All files hides metadata companions. */
    app.top_content = TOP_CONTENT_CONTEXT;
    app.selection_idle_frames = 0;
    count_reads = true;
    for (int i = 0; i < 7; i++)
        top_content_tick(&app);
    count_reads = false;
    assert(!opens && app.top.stage == 0);
    prepare();
    assert(app.top.metadata_present && !app.top.metadata_invalid);
    assert(!strcmp(app.top.title, "星の図書館") && !strcmp(app.top.year, "2026"));
    assert(!strcmp(app.top.handler, "No file association"));
    assert(app.top.files == 1 && !app.top.folders && app.top.types[PLACEHOLDER_TEXT] == 1);
    count_reads = true;
    unsigned reads = opens;
    for (int i = 0; i < 1000; i++)
        top_content_tick(&app);
    count_reads = false;
    assert(opens == reads); /* No repeat probing of missing files. */
    draw();
    assert(app_save(&app));
    app_init(&restored, argv[1], true, true);
    assert(restored.top_content == TOP_CONTENT_SMART);
    assert(appearance_save(&app, 0));
    app.top_content = TOP_CONTENT_NEIGHBORS;
    assert(appearance_load(&app, 0) && app.top_content == TOP_CONTENT_SMART);

    const char *bad[] = {"version=2\ntitle=Bad",          "version=1\ntitle=First\ntitle=Second",
                         "version=1\nyear=202A",          "version=1\nunknown=value",
                         "version=1\ntitle=bad\tcontrol", "version=1\ntitle=\xFF"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        write_text("roms/Fixture.txt.gloam.ini", bad[i]);
        prepare();
        assert(app.top.metadata_invalid && !app.top.metadata_present && !*app.top.title);
        draw();
    }
    char sidecar[PATH_CAP];
    assert(path_join(sidecar, app.root, "roms/Fixture.txt.gloam.ini"));
    FILE *f = fopen(sidecar, "wb");
    const char binary[] = "version=1\ntitle=Hidden\0suffix";
    assert(f && fwrite(binary, 1, sizeof binary, f) == sizeof binary && !fclose(f));
    prepare();
    assert(app.top.metadata_invalid);
    f = fopen(sidecar, "wb");
    assert(f);
    for (unsigned i = 0; i < 3000; i++)
        assert(fputc('X', f) != EOF);
    assert(!fclose(f));
    prepare();
    assert(app.top.metadata_invalid);
    assert(!remove(sidecar));
    prepare();
    assert(!app.top.metadata_present && !app.top.metadata_invalid);

    app.association_count = 1;
    path_copy(app.associations[0].extensions, "txt", sizeof app.associations[0].extensions);
    path_copy(app.associations[0].name, "Example reader", sizeof app.associations[0].name);
    assert(path_join(app.associations[0].application, app.root, "roms/missing.nds"));
    prepare();
    assert(!strcmp(app.top.handler, "Example reader") && strstr(app.top.route, "App missing"));
    path_copy(app.associations[0].application, app.entries[0].path, PATH_CAP);
    prepare();
    assert(strstr(app.top.route, "App available"));
    app.associations[1] = app.associations[0];
    assert(path_join(app.associations[1].application, app.root, "roms/another.nds"));
    app.association_count = 2;
    prepare();
    assert(!strcmp(app.top.handler, "Choose app on launch"));
    app.association_count = 0;

    assert(path_join(app.entries[0].path, app.root, "roms/Fixture.nds"));
    path_copy(app.entries[0].name, "Fixture.nds", sizeof app.entries[0].name);
    write_text("roms/Fixture.nds", "Not a runnable ROM; options fixture only");
    GameOptions options;
    game_options_defaults(&options, true);
    options.values[GAME_SLOT] = 3;
    assert(game_options_write(app.root, app.entries[0].path, &options));
    prepare();
    assert(strstr(app.top.options, "Slot 3") && strstr(app.top.options, "1 overrides"));
    assert(strstr(app.top.save, "not found"));
    char saves[PATH_CAP];
    assert(path_join(saves, app.root, "roms/saves") && !mkdir(saves, 0777));
    write_text("roms/saves/Fixture.sav3", "Original test save");
    prepare();
    assert(strstr(app.top.save, "file present"));
    options.values[GAME_SLOT] = 0;
    assert(game_options_write(app.root, app.entries[0].path, &options));
    write_text("roms/Fixture.sav", "First slot-zero fixture");
    write_text("roms/saves/Fixture.sav", "Second slot-zero fixture");
    prepare();
    assert(strstr(app.top.save, "multiple paths"));

    for (unsigned mode = 1; mode < TOP_CONTENT_COUNT; mode++) {
        app.top_content = mode;
        app_reset_preview(&app);
        assert(!*app.top.title && app.top.stage == 0);
        for (unsigned layout = 0; layout < 10; layout++)
            for (unsigned tabs = 0; tabs < 4; tabs++)
                for (unsigned view = 0; view < BROWSER_STYLE_COUNT; view++) {
                    app.themes.active.top_layout = layout;
                    app.themes.active.tab_style = tabs;
                    app.browser_style = view;
                    for (unsigned size = 0; size < ICON_SIZE_COUNT; size++) {
                        app.icon_size = size;
                        app.detail_tab = 0;
                        draw();
                        app.detail_tab = 1;
                        draw();
                    }
                }
    }
    app.detail_tab = 0;
    app.top_content = TOP_CONTENT_SMART;
    assert(top_content_mode(&app) == TOP_CONTENT_CONTEXT);
    app.entries[0].kind = ITEM_DIRECTORY;
    assert(top_content_mode(&app) == TOP_CONTENT_FOLDER);
    app.category = CATEGORY_HOME;
    assert(top_content_mode(&app) == TOP_CONTENT_ACTIVITY);
    app.recovery_mode = true;
    assert(top_content_mode(&app) == TOP_CONTENT_THEME);
    app.recovery_mode = false;
    app.top_content = TOP_CONTENT_THEME;
    app.themes.active.top_content = TOP_CONTENT_NEIGHBORS;
    assert(top_content_mode(&app) == TOP_CONTENT_NEIGHBORS);
    app.themes.active.top_content = 999;
    assert(top_content_mode(&app) == TOP_CONTENT_THEME);
    app_reset_preview(&app);
    app.top_content = TOP_CONTENT_FOLDER;
    app.browser_style = BROWSER_SHELF;
    app.show_covers = true;
    app.artwork_resolved = true;
    app.selection_idle_frames = 7;
    app.entry_count = 5;
    app.locations[app.category].selected = 3;
    app.top.stage = 1;
    for (int i = 0; i < 3; i++)
        app.top.samples[i] = i;
    for (int i = 1; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof name, "Fixture-%d.txt", i);
        char folder[PATH_CAP];
        assert(path_join(folder, app.root, "roms"));
        assert(path_join(app.entries[i].path, folder, name));
        path_copy(app.entries[i].name, name, sizeof app.entries[i].name);
        app.entries[i].kind = ITEM_FILE;
    }
    for (int i = 0; i < 4; i++)
        artwork_shelf_tick(&app);
    for (int i = 2; i <= 4; i++) {
        bool cached = false;
        for (unsigned slot = 0; slot < 3; slot++)
            cached |= !strcmp(app.shelf_paths[slot], app.entries[i].path);
        assert(cached);
    }

    char theme_root[PATH_CAP], theme_folder[PATH_CAP];
    assert(path_join(theme_root, app.root, "_nds/dsi-frontend/themes") && !mkdir(theme_root, 0777));
    assert(path_join(theme_folder, theme_root, "context") && !mkdir(theme_folder, 0777));
    write_text("_nds/dsi-frontend/themes/context/theme.ini",
               "[theme]\nversion=1\nname=Context fixture\n[layout]\ntop_content=context\n");
    app.top_content = TOP_CONTENT_THEME;
    path_copy(app.theme_id, "context", sizeof app.theme_id);
    themes_init(&app);
    assert(app.themes.active.top_content == TOP_CONTENT_CONTEXT);
    assert(top_content_mode(&app) == TOP_CONTENT_CONTEXT);
    app.top_content = TOP_CONTENT_ORIGINAL;
    assert(top_content_mode(&app) == TOP_CONTENT_THEME);
    write_text("_nds/dsi-frontend/themes/context/theme.ini",
               "[theme]\nversion=1\nname=Context fixture\n[layout]\ntop_content=unknown\n");
    themes_init(&app);
    assert(!*app.themes.active.id && app.themes.active.top_content == TOP_CONTENT_THEME);
    puts("PASS: lazy top content, bounded UTF-8 metadata, idle/read-free rendering, "
         "persistence/recovery, and all layout/view/tab/icon combinations");
    return 0;
}
