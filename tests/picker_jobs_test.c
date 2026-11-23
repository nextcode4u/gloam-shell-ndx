#include "app.h"
#include "directory_scan.h"
#include "paths.h"
#include "picker.h"
#include "renderer.h"
#include "storage_state.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static App app;
static unsigned scan_opens, read_attempts;
static int fail_read_after = -1;
bool __real_directory_scan_read(void *, DirectoryRecord *);
bool __wrap_directory_scan_read(void *directory, DirectoryRecord *record)
{
    if (fail_read_after >= 0 && read_attempts++ == (unsigned)fail_read_after) {
        errno = EIO;
        return false;
    }
    return __real_directory_scan_read(directory, record);
}
void *__real_directory_scan_open(const char *);
void *__wrap_directory_scan_open(const char *path)
{
    scan_opens++;
    return __real_directory_scan_open(path);
}
static uint16_t top[256 * 192], bottom[256 * 192];
static void touch_file(const char *directory, const char *name)
{
    char path[PATH_CAP];
    assert(path_join(path, directory, name));
    FILE *file = fopen(path, "w");
    assert(file && !fclose(file));
}
static void finish_bounded_scan(void)
{
    while (app.picker.directory) {
        unsigned before = app.picker.visited;
        assert(app.picker.count == 0); /* Never publish a partially sorted batch. */
        picker_scan_step(&app);
        if (app.picker.visited < before) {
            assert(!app.picker.visited && !app.picker.batch && app.picker.directory);
        } else
            assert(app.picker.visited - before <= 16);
    }
}
static void later_parent_batches(const char *roms)
{
    char parent[PATH_CAP], target[PATH_CAP], child[PATH_CAP], folder[PATH_CAP];
    assert(path_join(parent, roms, "Navigation") && !mkdir(parent, 0777));
    for (unsigned i = 0; i < ENTRY_CAP * 2 + 7; i++) {
        char name[32];
        snprintf(name, sizeof name, "Folder%04u", i);
        assert(path_join(folder, parent, name) && !mkdir(folder, 0777));
    }
    assert(picker_open(&app, PICKER_COVER));
    path_copy(app.picker.path, parent, PATH_CAP);
    picker_scan_begin(&app);
    finish_bounded_scan();
    for (unsigned batch = 1; batch <= 2; batch++) {
        picker_batch(&app, 1);
        assert(app.picker.batch == batch && app.picker.count > 3);
        picker_move(&app, app.picker.count - 2);
        path_copy(target, app.picker.entries[app.picker.selected].path, PATH_CAP);
        assert(path_join(child, target, "Child") && !mkdir(child, 0777));
        picker_activate(&app);
        assert(!strcmp(app.picker.path, target) && app.picker.count == 1);
        picker_activate(&app);
        assert(!strcmp(app.picker.path, child));
        picker_back(&app);
        assert(!strcmp(app.picker.path, target) &&
               !strcmp(app.picker.entries[app.picker.selected].path, child));
        unsigned opened = scan_opens;
        picker_back(&app);
        assert(!strcmp(app.picker.path, parent) && app.picker.batch == batch &&
               !strcmp(app.picker.entries[app.picker.selected].path, target));
        assert(scan_opens == opened + 1); /* One pass, never one reopen per skipped batch. */
        assert(app.picker.selected >= app.picker.scroll &&
               app.picker.selected < app.picker.scroll + PICKER_VISIBLE_ROWS);
        /* Exercise the cooperative path without HOST_PREVIEW's synchronous refresh. */
        app.picker.batch = 0;
        opened = scan_opens;
        picker_scan_begin(&app);
        path_copy(app.picker.restore_path, target, PATH_CAP);
        finish_bounded_scan();
        assert(scan_opens == opened + 1 && app.picker.batch == batch &&
               !strcmp(app.picker.entries[app.picker.selected].path, target));
    }
    /* A read error during restoration discards the batch instead of retrying. */
    unsigned opened = scan_opens;
    app.picker.batch = 0;
    picker_scan_begin(&app);
    path_copy(app.picker.restore_path, target, PATH_CAP);
    read_attempts = 0;
    fail_read_after = ENTRY_CAP + 30;
    finish_bounded_scan();
    fail_read_after = -1;
    assert(app.picker.failed && !app.picker.count && !app.picker.directory &&
           scan_opens == opened + 1);
    app.picker.batch = 0;
    picker_scan_begin(&app);
    path_copy(app.picker.restore_path, target, PATH_CAP);
    finish_bounded_scan();
    assert(!app.picker.failed && app.picker.batch == 2 &&
           !strcmp(app.picker.entries[app.picker.selected].path, target));
    /* A removed return target falls back once to a usable first batch. */
    picker_activate(&app);
    assert(!rmdir(child) && !rmdir(target));
    opened = scan_opens;
    picker_back(&app);
    assert(scan_opens == opened + 2 && app.picker.batch == 0 && app.picker.count == ENTRY_CAP &&
           !app.picker.failed && !*app.picker.restore_path);
    /* The fallback also stays within the native per-frame record budget. */
    opened = scan_opens;
    picker_scan_begin(&app);
    path_copy(app.picker.restore_path, target, PATH_CAP);
    finish_bounded_scan();
    assert(scan_opens == opened + 2 && !app.picker.batch && app.picker.count == ENTRY_CAP);
    picker_close(&app);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    char roms[PATH_CAP], bulk[PATH_CAP];
    assert(path_join(roms, argv[1], "roms") && !mkdir(roms, 0777));
    assert(path_join(bulk, roms, "Bulk") && !mkdir(bulk, 0777));
    for (int i = 0; i < 1000; i++) {
        char name[32];
        snprintf(name, sizeof name, "Image%04d.png", i);
        touch_file(bulk, name);
        snprintf(name, sizeof name, "Filtered%04d.txt", i);
        touch_file(bulk, name);
    }
    touch_file(bulk, ".Hidden.png");
    app_init(&app, argv[1], true, true);
    app_category(&app, CATEGORY_GAMES);
    Location origin = app.locations[CATEGORY_GAMES];
    assert(picker_open(&app, PICKER_COVER));
    picker_scan_begin(&app); /* Exercise the native incremental path explicitly. */
    assert(app.picker.directory && !app.picker.count);
    finish_bounded_scan();
    assert(app.picker.count == 512 && app.picker.truncated);
    bool seen[1000] = {false};
    for (int batch = 0; batch < 2; batch++) {
        for (int i = 0; i < app.picker.count; i++) {
            int number = -1;
            assert(sscanf(app.picker.entries[i].name, "Image%d.png", &number) == 1);
            assert(number >= 0 && number < 1000 && !seen[number]);
            seen[number] = true;
        }
        if (batch == 0) {
            app.picker.confirmed = true;
            picker_batch(&app, 1);
            assert(app.picker.batch == 1 && !app.picker.confirmed);
            picker_scan_begin(&app);
            finish_bounded_scan();
            assert(app.picker.count == 488 && !app.picker.truncated);
        }
    }
    for (int i = 0; i < 1000; i++)
        assert(seen[i]);
    render_app(&app, top, bottom, "12:00");
    picker_touch(&app, 240, 8); /* Last batch wraps to the first. */
    assert(app.picker.batch == 0 && app.picker.count == 512);
    picker_batch(&app, -1);
    assert(app.picker.batch == 0);
    picker_back(&app);
    assert(!strcmp(app.picker.path, roms) && app.picker.batch == 0);
    assert(!strcmp(app.picker.entries[app.picker.selected].path, bulk));
    picker_activate(&app);
    picker_scan_begin(&app);
    assert(app.picker.directory);
    picker_close(&app);
    assert(!app.picker.directory && !app.picker.entries && !app.picker.open);
    assert(!memcmp(&origin, &app.locations[CATEGORY_GAMES], sizeof origin));
    later_parent_batches(roms);
    assert(!memcmp(&origin, &app.locations[CATEGORY_GAMES], sizeof origin));
    assert(picker_open(&app, PICKER_COVER));
    picker_scan_begin(&app);
    assert(app.picker.directory);
    storage_observe(&app, false);
    assert(app.storage_lost && !app.picker.directory && !app.picker.open);
    puts(
        "PASS: bounded picker scans, hidden/filtered work, 1000-file batch coverage, touch paging, "
        "confirmation reset, parent selection, cancellation and SD-loss cleanup");
}
