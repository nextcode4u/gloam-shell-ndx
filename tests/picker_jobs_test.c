#include "app.h"
#include "paths.h"
#include "picker.h"
#include "renderer.h"
#include "storage_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app;
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
        assert(app.picker.visited - before <= 16);
    }
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
    assert(picker_open(&app, PICKER_COVER));
    picker_scan_begin(&app);
    assert(app.picker.directory);
    storage_observe(&app, false);
    assert(app.storage_lost && !app.picker.directory && !app.picker.open);
    puts(
        "PASS: bounded picker scans, hidden/filtered work, 1000-file batch coverage, touch paging, "
        "confirmation reset, parent selection, cancellation and SD-loss cleanup");
}
