#include "app.h"
#include "appearance.h"
#include "browser.h"
#include "collections.h"
#include "paths.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
/* Inject failures only in the selected record, leaving fixtures and assets intact. */
static char watched_path[PATH_CAP];
static FILE *watched_file;
static bool fail_close, fail_metadata, fail_open;
static int lines_left = -1;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_fstat(int, struct stat *);
char *__real_fgets(char *, int, FILE *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (fail_open && !strcmp(path, watched_path) && *mode == 'r') {
        errno = EIO;
        return NULL;
    }
    FILE *file = __real_fopen(path, mode);
    if (*watched_path && !strcmp(path, watched_path) && *mode == 'r')
        watched_file = file;
    return file;
}
int __wrap_fclose(FILE *file)
{
    bool deny = file == watched_file && fail_close;
    if (file == watched_file)
        watched_file = NULL;
    int result = __real_fclose(file);
    if (deny) {
        errno = EIO;
        return EOF;
    }
    return result;
}
int __wrap_fstat(int descriptor, struct stat *status)
{
    if (watched_file && descriptor == fileno(watched_file) && fail_metadata) {
        errno = EIO;
        return -1;
    }
    return __real_fstat(descriptor, status);
}
char *__wrap_fgets(char *text, int capacity, FILE *file)
{
    if (file == watched_file && lines_left >= 0) {
        if (!lines_left)
            return NULL; /* Premature EOF without ferror, after a valid prefix. */
        lines_left--;
    }
    return __real_fgets(text, capacity, file);
}
#ifdef __GLIBC__
char *__wrap___fgets_chk(char *text, size_t capacity, int count, FILE *file)
{
    assert(count > 0 && (size_t)count <= capacity);
    return __wrap_fgets(text, count, file);
}
#endif
static App app;
static void write_config(const char *root, const char *text)
{
    char path[PATH_CAP];
    assert(path_preferences_file(path, root, "collections.ini"));
    FILE *f = fopen(path, "w");
    assert(f && fputs(text, f) >= 0 && !fclose(f));
}
static void read_failures(const char *root)
{
    static const char collection[] =
        "[collections]\nversion=1\n[collection.games]\nitem0=system:settings\nitem1=system:menu\n";
    write_config(root, collection);
    assert(path_preferences_file(watched_path, root, "collections.ini"));
    for (int fault = 0; fault < 4; fault++) {
        fail_open = fault == 3;
        fail_close = fault == 0;
        fail_metadata = fault == 1;
        lines_left = fault == 2 ? 4 : -1;
        collections_load(&app);
        assert(app.collections.invalid && !app.collections.count && !app.collections.item_count);
    }
    fail_close = fail_metadata = fail_open = false;
    lines_left = -1;
    collections_load(&app);
    assert(!app.collections.invalid && app.collections.count == 1 &&
           app.collections.item_count == 2);
    /* Hidden bytes after otherwise valid text must not disappear into strlen. */
    FILE *file = fopen(watched_path, "ab");
    assert(file && fwrite("\0hidden", 1, 7, file) == 7 && !fclose(file));
    collections_load(&app);
    assert(app.collections.invalid && !app.collections.count);

    static const char preset[] =
        "version=1\r\nname=Travel\r\ntheme=\r\nvalues=0,0,0,0,0,1,0,-1\r\ntop=\r\nbottom=\r\n"
        "icon_labels=0\r\n";
    assert(path_preferences_file(watched_path, root, "appearance-1.ini"));
    file = fopen(watched_path, "wb");
    assert(file && fwrite(preset, 1, sizeof preset - 1, file) == sizeof preset - 1 &&
           !fclose(file));
    app.show_icon_labels = true;
    app.wallpaper_strength = 17;
    for (int fault = 0; fault < 4; fault++) {
        fail_open = fault == 3;
        fail_close = fault == 0;
        fail_metadata = fault == 1;
        lines_left = fault == 2 ? 6 : -1;
        assert(!appearance_load(&app, 0));
        assert(app.show_icon_labels && app.wallpaper_strength == 17);
    }
    fail_close = fail_metadata = fail_open = false;
    lines_left = -1;
    assert(appearance_load(&app, 0) && !app.show_icon_labels && app.wallpaper_strength == -1);
    file = fopen(watched_path, "ab");
    assert(file && fwrite("\0hidden", 1, 7, file) == 7 && !fclose(file));
    app.show_icon_labels = true;
    assert(!appearance_load(&app, 0) && app.show_icon_labels);
    watched_path[0] = 0;
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
    read_failures(argv[1]);
    app_init_mode(&app, argv[1], true, true, true);
    assert(!app.collections.count && app.collection_open == -1);
    puts("PASS: named collections, shared bounded targets, Home/back navigation, missing paths, "
         "malformed config and recovery");
}
