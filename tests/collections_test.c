#include "app.h"
#include "browser.h"
#include "collections.h"
#include "paths.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static App app;
static void write_config(const char *root, const char *text)
{
    char path[PATH_CAP];
    assert(path_preferences_file(path, root, "collections.ini"));
    FILE *f = fopen(path, "w");
    assert(f && fputs(text, f) >= 0 && !fclose(f));
}
int main(int argc, char **argv)
{
    assert(argc == 2 && path_prepare_preferences_directory(argv[1]));
    write_config(argv[1], "[collections]\nversion=1\n[collection.games]\nlabel=Evening "
                          "games\nitem0=sd:/"
                          "missing.nds\nitem1=system:settings\n[collection.tools]\nlabel="
                          "Tools\nitem0=system:settings\n");
    app_init(&app, argv[1], true, true);
    assert(app.collections.count == 2 && app.collections.item_count == 2 &&
           app.collection_open == -1);
    int selection = -1;
    for (int i = 0; i < app.entry_count; i++)
        if (app.entries[i].kind == ITEM_CATEGORY && app.entries[i].target == -1)
            selection = i;
    assert(selection >= 0);
    app.locations[CATEGORY_HOME].selected = selection;
    app_open(&app);
    assert(app.collection_open == 0 && app.entry_count == 2 && app.entries[0].kind == ITEM_MISSING);
    app_open(&app);
    assert(app.panel == PANEL_MESSAGE && !app.picker.open);
    app_back(&app);
    app_back(&app);
    assert(app.collection_open == -1 && app.locations[CATEGORY_HOME].selected == selection);
    app_open(&app);
    app_category(&app, CATEGORY_SYSTEM);
    assert(app.collection_open == -1);
    write_config(argv[1], "[collections]\nversion=1\n[collection.bad]\nitem0=sd:/../outside\n");
    collections_load(&app);
    assert(app.collections.invalid && !app.collections.count);
    write_config(
        argv[1],
        "[collections]\nversion=1\n[collection.bad]\nitem0=system:settings\nitem0=system:menu\n");
    collections_load(&app);
    assert(app.collections.invalid && !app.collections.count);
    write_config(argv[1], "[collections]\nversion=1\n[collection.valid]\nitem0=system:settings\n");
    char folder[PATH_CAP];
    assert(path_join(folder, argv[1], "collection-folder") && !mkdir(folder, 0777));
    write_config(argv[1],
                 "[collections]\nversion=1\n[collection.folder]\nitem0=sd:/collection-folder\n");
    app_init(&app, argv[1], true, true);
    for (int i = 0; i < app.entry_count; i++)
        if (app.entries[i].kind == ITEM_CATEGORY && app.entries[i].target == -1)
            app.locations[CATEGORY_HOME].selected = i;
    app_open(&app);
    app_open(&app);
    assert(app.category == CATEGORY_FILES && app.collection_open == -1);
    app_init_mode(&app, argv[1], true, true, true);
    assert(!app.collections.count && app.collection_open == -1);
    puts("PASS: named collections, shared bounded targets, Home/back navigation, missing paths, "
         "malformed config and recovery");
}
