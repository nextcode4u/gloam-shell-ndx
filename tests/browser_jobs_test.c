#include "app.h"
#include "browser.h"
#include "directory_scan.h"
#include "paths.h"
#include "preferences.h"
#include "storage_state.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static App app, restored;
static unsigned fixture_count = 1000;
static int fail_after = -1;
static unsigned read_calls, total_reads, scan_opens;
void *__real_directory_scan_open(const char *);
void *__wrap_directory_scan_open(const char *path)
{
    scan_opens++;
    return __real_directory_scan_open(path);
}
static bool stale_errno;
static char metadata_failure[PATH_CAP];
static int metadata_error;
int __real_stat(const char *, struct stat *);
int __wrap_stat(const char *path, struct stat *status)
{
    if (metadata_error && !strcmp(path, metadata_failure)) {
        errno = metadata_error;
        return -1;
    }
    return __real_stat(path, status);
}
bool __real_directory_scan_read(void *, DirectoryRecord *);
bool __wrap_directory_scan_read(void *directory, DirectoryRecord *record)
{
    total_reads++;
    if (fail_after >= 0 && read_calls++ >= (unsigned)fail_after) {
        errno = EIO;
        return false;
    }
    bool read = __real_directory_scan_read(directory, record);
    if (read && stale_errno)
        errno = EIO; /* A skipped entry's metadata can leave an unrelated error. */
    return read;
}
static void drain(void)
{
    unsigned frames = 0;
    while (app.scan_directory) {
        unsigned before = app.scan_visited;
        assert(app.entry_count == 0); /* Never publish partially sorted snapshots. */
        browser_scan_step(&app);
        assert(app.scan_visited < before || app.scan_visited - before <= 16);
        assert(++frames < fixture_count / 16 + 200);
    }
}
static void read_failures(void)
{
    Location *location = &app.locations[CATEGORY_GAMES];
    const int failures[] = {0, 24};
    for (unsigned batch = 0; batch < 2; batch++)
        for (unsigned i = 0; i < sizeof failures / sizeof *failures; i++) {
            location->batch = batch;
            location->selected = 7;
            location->scroll = 3;
            app.preferences_dirty = false;
            app.panel = PANEL_NONE;
            browser_scan_begin(&app);
            app.scan_seek_all = true;
            strcpy(app.scan_restore_path, "missing-search-target");
            read_calls = 0;
            fail_after = failures[i];
            drain();
            fail_after = -1;
            assert(app.directory_error && !app.entry_count && !app.scan_count);
            assert(!app.scan_directory && !app.scan_seek_all && !*app.scan_restore_path);
            assert(!app.truncated && app.panel == PANEL_NONE && !app.preferences_dirty);
            assert(location->batch == batch && location->selected == 7 && location->scroll == 3);
            browser_scan_begin(&app);
            drain();
            assert(!app.directory_error && app.entry_count && location->batch == batch);
        }
    location->batch = 1;
    location->selected = 7;
    location->scroll = 3;
    read_calls = 0;
    fail_after = 24;
    browser_refresh(&app);
    drain();
    fail_after = -1;
    assert(app.directory_error && !app.entry_count);
    assert(location->batch == 1 && location->selected == 7 && location->scroll == 3);
    browser_refresh(&app);
    drain();
    assert(!app.directory_error && app.entry_count && location->selected == 7);
    location->batch = 0;
    browser_scan_begin(&app);
    drain();
}
static void metadata_failures(void)
{
    Location *location = &app.locations[CATEGORY_GAMES];
    assert(app.entry_count > 7);
    path_copy(metadata_failure, app.entries[7].path, PATH_CAP);
    const int errors[] = {EIO, EACCES, ENOTDIR};
    for (unsigned i = 0; i < sizeof errors / sizeof *errors; i++) {
        location->selected = 7;
        location->scroll = 3;
        app.preferences_dirty = false;
        browser_scan_begin(&app);
        metadata_error = errors[i];
        drain();
        metadata_error = 0;
        assert(app.directory_error && !app.entry_count && !app.scan_count && !app.scan_directory &&
               !app.truncated && !app.preferences_dirty);
        assert(location->selected == 7 && location->scroll == 3 && !location->batch);
        browser_scan_begin(&app);
        drain();
        assert(!app.directory_error && app.entry_count == ENTRY_CAP);
    }
    /* Removing a single entry during enumeration does not invalidate the rest. */
    browser_scan_begin(&app);
    metadata_error = ENOENT;
    drain();
    metadata_error = 0;
    assert(!app.directory_error && app.entry_count == ENTRY_CAP);
    for (int i = 0; i < app.entry_count; i++)
        assert(strcmp(app.entries[i].path, metadata_failure));
    browser_scan_begin(&app);
    drain();
    assert(!app.directory_error && app.entry_count == ENTRY_CAP);
}
static void locate_one_pass(const char *roms)
{
    char folder[PATH_CAP], path[PATH_CAP], first[PATH_CAP] = "", last[PATH_CAP] = "";
    assert(path_join(folder, roms, "locate-control") && !mkdir(folder, 0777));
    const unsigned count = ENTRY_CAP * 3 + 7;
    for (unsigned i = 0; i < count; i++) {
        char name[32];
        snprintf(name, sizeof name, "Locate%04u.txt", i);
        assert(path_join(path, folder, name));
        FILE *file = fopen(path, "w");
        assert(file && !fclose(file));
    }
    /* Choose targets by actual directory order, independently of the host FS. */
    void *directory = directory_scan_open(folder);
    assert(directory);
    DirectoryRecord record;
    while (directory_scan_read(directory, &record))
        if (record.valid && record.regular) {
            assert(path_join(last, folder, record.name));
            if (!*first)
                path_copy(first, last, PATH_CAP);
        }
    directory_scan_close(directory);
    assert(*first && *last);
    Location *location = &app.locations[CATEGORY_GAMES];
    for (unsigned sort = 0; sort < 4; sort++) {
        app.sort_mode = sort;
        location->batch = location->selected = location->scroll = 0;
        path_copy(location->path, folder, PATH_CAP);
        unsigned opened = scan_opens, reads = total_reads;
        browser_scan_begin(&app);
        app.scan_seek_all = true;
        path_copy(app.scan_restore_path, last, PATH_CAP);
        drain();
        assert(!app.directory_error && location->batch == 3 && app.entry_count == 7 &&
               app_selected(&app) && !strcmp(app_selected(&app)->path, last));
        assert(scan_opens == opened + 1 && total_reads - reads <= count + 3);
    }
    /* An early match stops after its batch, rather than traversing the folder. */
    location->batch = 0;
    unsigned opened = scan_opens, reads = total_reads;
    browser_scan_begin(&app);
    app.scan_seek_all = true;
    path_copy(app.scan_restore_path, first, PATH_CAP);
    drain();
    assert(location->batch == 0 && app.entry_count == ENTRY_CAP && app.truncated &&
           !strcmp(app_selected(&app)->path, first));
    assert(scan_opens == opened + 1 && total_reads - reads <= ENTRY_CAP + 3);
    /* Missing/filtered results finish once and retain the ordinary message. */
    location->batch = 0;
    app.panel = PANEL_NONE;
    opened = scan_opens;
    reads = total_reads;
    browser_scan_begin(&app);
    app.scan_seek_all = true;
    assert(path_join(app.scan_restore_path, folder, "Missing.txt"));
    drain();
    assert(!app.directory_error && app.panel == PANEL_MESSAGE && !app.scan_seek_all &&
           strstr(app.message, "not visible"));
    assert(scan_opens == opened + 1 && total_reads - reads <= count + 3);
    app.panel = PANEL_NONE;
    /* A failure after crossing a batch boundary still discards partial work. */
    location->batch = 0;
    browser_scan_begin(&app);
    app.scan_seek_all = true;
    path_copy(app.scan_restore_path, last, PATH_CAP);
    fail_after = ENTRY_CAP + 30;
    read_calls = 0;
    drain();
    fail_after = -1;
    assert(app.directory_error && !app.entry_count && !app.scan_count && !app.scan_seek_all);
    app.sort_mode = 0;
    location->batch = 0;
    path_copy(location->path, roms, PATH_CAP);
    browser_scan_begin(&app);
    drain();
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
    read_failures();
    metadata_failures();
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
    /* A saved later batch can disappear after the card's library is pruned.
     * Boot and refresh must recover to a real batch, not show an empty folder. */
    for (int i = 0; i < restored.entry_count; i++)
        assert(!remove(restored.entries[i].path));
    app_init(&restored, argv[1], true, true);
    assert(!restored.locations[CATEGORY_GAMES].batch && restored.entry_count == ENTRY_CAP);
    assert(restored.truncated == (fixture_count - (unsigned)last_count > ENTRY_CAP));
    assert(app_save(&restored));
    browser_refresh(&app);
    assert(!app.locations[CATEGORY_GAMES].batch && app.entry_count == ENTRY_CAP);
    while (app.locations[CATEGORY_GAMES].batch)
        browser_batch(&app, -1);
    assert(app.locations[CATEGORY_GAMES].batch == 0 && app.entry_count == ENTRY_CAP);
    browser_scan_begin(&app);
    app_category(&app, CATEGORY_HOME);
    assert(!app.scan_directory && app.category == CATEGORY_HOME);
    app_category(&app, CATEGORY_GAMES);
    char filtered[PATH_CAP];
    assert(path_join(filtered, folder, "filtered-control") && !mkdir(filtered, 0777));
    for (int i = 0; i < ENTRY_CAP + 1; i++) {
        char name[32];
        snprintf(name, sizeof name, "%04d.weird", i);
        assert(path_join(path, filtered, name));
        FILE *file = fopen(path, "w");
        assert(file && !fclose(file));
    }
    assert(path_join(path, filtered, "Visible.txt"));
    FILE *visible = fopen(path, "w");
    assert(visible && !fclose(visible));
    path_copy(app.locations[CATEGORY_GAMES].path, filtered, PATH_CAP);
    app_scan(&app);
    assert(app.entry_count == ENTRY_CAP && app.truncated);
    browser_batch(&app, 1);
    assert(app.locations[CATEGORY_GAMES].batch == 1 && app.entry_count == 2);
    app.filter_recognized = true;
    browser_scan_begin(&app);
    browser_restore_selection(&app, path);
    drain();
    assert(!app.locations[CATEGORY_GAMES].batch && app.entry_count == 1);
    assert(!strcmp(app_selected(&app)->path, path));
    /* An actually empty filtered folder must finish rather than restarting. */
    assert(!remove(path));
    app.locations[CATEGORY_GAMES].batch = 4095;
    browser_scan_begin(&app);
    drain();
    assert(!app.locations[CATEGORY_GAMES].batch && !app.entry_count && !app.scan_directory);
    app.filter_recognized = false;
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
    locate_one_pass(folder);
    path_copy(app.locations[CATEGORY_GAMES].path, restore_folder, PATH_CAP);
    browser_scan_begin(&app);
    stale_errno = true;
    drain();
    stale_errno = false;
    assert(!app.directory_error && app.entry_count == 4);
    browser_scan_begin(&app);
    storage_observe(&app, false);
    assert(!app.scan_directory && app.storage_lost);
    printf("PASS: bounded directory work, cancellation, unpublished partial snapshots, %u-file "
           "batch coverage, read-error discard/retry, stale/filtered batch recovery and persistent "
           "batch selection\n",
           fixture_count);
}
