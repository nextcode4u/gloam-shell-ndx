#include "app.h"
#include "browser.h"
#include "paths.h"
#include "preferences.h"
#include "storage_state.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static App app, restored;
static unsigned fixture_count = 1000;
static void drain(void)
{
    unsigned frames = 0;
    while (app.scan_directory) {
        unsigned before = app.scan_visited;
        assert(app.entry_count == 0); /* Never publish partially sorted snapshots. */
        browser_scan_step(&app);
        assert(app.scan_visited - before <= 16);
        assert(++frames < fixture_count / 16 + 200);
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2 || argc == 3);
    if (argc == 3) {
        char *end;
        unsigned long count = strtoul(argv[2], &end, 10);
        assert(!*end && count >= 1000 && count <= 100000);
        fixture_count = (unsigned)count;
    }
    char folder[PATH_CAP], path[PATH_CAP];
    assert(path_join(folder, argv[1], "roms") && !mkdir(folder, 0777));
    for (unsigned i = 0; i < fixture_count; i++) {
        char name[32];
        snprintf(name, sizeof name, "%04d.txt", i);
        assert(path_join(path, folder, name));
        FILE *file = fopen(path, "w");
        assert(file && !fclose(file));
    }
    app_init(&app, argv[1], true, true);
    app_category(&app, CATEGORY_GAMES);
    assert(app.entry_count == 512 && app.truncated);
    browser_scan_begin(&app);
    browser_scan_step(&app);
    assert(app.scan_directory && app.scan_visited <= 16 && app.entry_count == 0);
    browser_scan_cancel(&app);
    assert(!app.scan_directory && !app.scan_count);
    browser_scan_begin(&app);
    drain();
    assert(app.entry_count == 512 && app.truncated);
    bool *seen = calloc(fixture_count, sizeof *seen);
    assert(seen);
    for (int i = 0; i < app.entry_count; i++) {
        unsigned number = (unsigned)atoi(app.entries[i].name);
        assert(number < fixture_count && !seen[number]);
        seen[number] = true;
    }
    while (app.truncated) {
        unsigned previous = app.locations[CATEGORY_GAMES].batch;
        browser_batch(&app, 1);
        assert(app.locations[CATEGORY_GAMES].batch == previous + 1);
        unsigned remaining = fixture_count - app.locations[CATEGORY_GAMES].batch * ENTRY_CAP;
        assert(app.entry_count == (int)(remaining > ENTRY_CAP ? ENTRY_CAP : remaining));
        assert(app.truncated == (remaining > ENTRY_CAP));
        for (int i = 0; i < app.entry_count; i++) {
            unsigned number = (unsigned)atoi(app.entries[i].name);
            assert(number < fixture_count && !seen[number]);
            seen[number] = true;
        }
    }
    for (unsigned i = 0; i < fixture_count; i++)
        assert(seen[i]);
    free(seen);
    unsigned last_batch = app.locations[CATEGORY_GAMES].batch;
    int last_count = app.entry_count;
    assert(app_save(&app));
    app_init(&restored, argv[1], true, true);
    assert(restored.locations[CATEGORY_GAMES].batch == last_batch &&
           restored.entry_count == last_count);
    while (app.locations[CATEGORY_GAMES].batch)
        browser_batch(&app, -1);
    assert(app.locations[CATEGORY_GAMES].batch == 0 && app.entry_count == ENTRY_CAP);
    browser_scan_begin(&app);
    app_category(&app, CATEGORY_HOME);
    assert(!app.scan_directory && app.category == CATEGORY_HOME);
    app_category(&app, CATEGORY_GAMES);
    char restore_folder[PATH_CAP];
    assert(path_join(restore_folder, folder, "restore-control") && !mkdir(restore_folder, 0777));
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof name, "%d.txt", i);
        assert(path_join(path, restore_folder, name));
        FILE *file = fopen(path, "w");
        assert(file && !fclose(file));
    }
    path_copy(app.locations[CATEGORY_GAMES].path, restore_folder, PATH_CAP);
    app_scan(&app);
    app.locations[CATEGORY_GAMES].selected = 1;
    char selected_path[PATH_CAP];
    path_copy(selected_path, app_selected(&app)->path, sizeof selected_path);
    assert(app_save(&app));
    assert(path_join(path, restore_folder, "saves") && !mkdir(path, 0777));
    app_init(&restored, argv[1], true, true);
    assert(!strcmp(app_selected(&restored)->path, selected_path));
    assert(restored.entry_count == 3 && restored.locations[CATEGORY_GAMES].selected == 1);
    app.hide_save_folders = false;
    assert(app_save(&app));
    app_init(&restored, argv[1], true, true);
    assert(restored.entry_count == 4 && restored.locations[CATEGORY_GAMES].selected == 2);
    browser_scan_begin(&app);
    storage_observe(&app, false);
    assert(!app.scan_directory && app.storage_lost);
    printf("PASS: bounded directory work, cancellation, unpublished partial snapshots, %u-file "
           "batch coverage and persistent batch selection\n",
           fixture_count);
}
