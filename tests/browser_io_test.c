/* Browser display reuse must not weaken fresh launch/edit checks. */
#include "app.h"
#include "artwork.h"
#include "browser.h"
#include "icons.h"
#include "item_preferences.h"
#include "paths.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static unsigned record_opens, cover_stats;
static bool fail_records;
FILE *__real_fopen(const char *, const char *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (strstr(path, "/items/") && !strcmp(mode, "rb")) {
        record_opens++;
        if (fail_records) {
            errno = EIO;
            return NULL;
        }
    }
    return __real_fopen(path, mode);
}
int __real_stat(const char *, struct stat *);
int __wrap_stat(const char *path, struct stat *info)
{
    if (strstr(path, ".png") || strstr(path, ".jpg") || strstr(path, ".jpeg"))
        cover_stats++;
    return __real_stat(path, info);
}
static App app;
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    char content[PATH_CAP], missing[PATH_CAP], other[PATH_CAP], path[PATH_CAP];
    assert(path_join(content, root, "roms/One.demo"));
    assert(path_join(missing, root, "roms/Missing.demo"));
    assert(path_join(other, root, "roms/Two.demo"));
    ItemPreferences item = {0}, loaded;
    assert(path_join(item.cover, root, "assigned.png"));
    assert(item_preferences_save(root, content, &item));
    for (int i = 0; i < 3000; i++) {
        char relative[64];
        snprintf(relative, sizeof relative, "_nds/dsi-frontend/items/unrelated-%04d.ini", i);
        assert(path_join(path, root, relative));
        FILE *f = fopen(path, "w");
        assert(f && !fclose(f));
    }
    item_preferences_display_clear();
    record_opens = 0;
    assert(item_preferences_display(root, content, &loaded));
    unsigned first = record_opens;
    assert(first == 1);
    for (int i = 0; i < 20; i++)
        assert(item_preferences_display(root, content, &loaded) &&
               !strcmp(loaded.cover, item.cover));
    assert(record_opens == first);
    assert(!item_preferences_display(root, missing, &loaded));
    first = record_opens;
    assert(first == 3);
    for (int i = 0; i < 20; i++)
        assert(!item_preferences_display(root, missing, &loaded));
    assert(record_opens == first && !*loaded.cover);
    /* Cached display data is never an authorization to launch or overwrite. */
    fail_records = true;
    assert(!item_preferences_resolve(root, content, &loaded));
    assert(!item_preferences_read_for_edit(root, content, &loaded));
    item_preferences_display_clear();
    first = record_opens;
    assert(!item_preferences_display(root, other, &loaded));
    assert(!item_preferences_display(root, other, &loaded));
    assert(record_opens == first + 4); /* Transient failures are retried. */
    fail_records = false;
    assert(path_join(item.cover, root, "updated.png"));
    assert(item_preferences_save(root, content, &item));
    assert(item_preferences_display(root, content, &loaded) && !strcmp(loaded.cover, item.cover));
    assert(path_join(item.cover, root, "new.png"));
    assert(item_preferences_save(root, content, &item));
    assert(item_preferences_display(root, content, &loaded) && !strcmp(loaded.cover, item.cover));
    assert(item_preferences_display(root, content, &loaded));
    /* An explicit refresh drops the display cache, including negative entries. */
    icons_reset(&app);
    first = record_opens;
    assert(item_preferences_display(root, content, &loaded) && record_opens == first + 1);

    app_init(&app, root, true, true);
    app.category = CATEGORY_GAMES;
    app.entry_count = 1;
    app.entries[0].kind = ITEM_FILE;
    path_copy(app.entries[0].path, missing, PATH_CAP);
    app.locations[app.category].selected = 0;
    app_reset_preview(&app);
    unsigned steps = 0;
    while (!app.artwork_resolved && steps++ < 20) {
        cover_stats = 0;
        artwork_step(&app);
        assert(cover_stats <= 1); /* No chain of six missing image probes in one frame. */
    }
    assert(app.artwork_resolved && !app.artwork.ready && steps > 1 && steps < 20);
    assert(app.artwork_stage);
    app_reset_preview(&app);
    assert(!app.artwork_stage && !app.artwork_resolved);
    /* Completing the selected cover must not also start a neighbor read in
     * the same frame. Both have explicit assignments to make the extra I/O visible. */
    assert(path_join(path, root, "selected.png"));
    FILE *input = fopen("tests/fixtures/cover-example.png", "rb");
    FILE *output = fopen(path, "wb");
    assert(input && output);
    unsigned char buffer[512];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof buffer, input)))
        assert(fwrite(buffer, 1, bytes, output) == bytes);
    assert(!ferror(input) && !fclose(input) && !fclose(output));
    path_copy(item.cover, path, PATH_CAP);
    assert(item_preferences_save(root, content, &item));
    assert(path_join(item.cover, root, "neighbor.png"));
    assert(item_preferences_save(root, other, &item));
    app.entry_count = 2;
    app.entries[0].kind = app.entries[1].kind = ITEM_FILE;
    path_copy(app.entries[0].path, other, PATH_CAP);
    path_copy(app.entries[1].path, content, PATH_CAP);
    app.locations[app.category].selected = 1;
    app.top_content = TOP_CONTENT_NEIGHBORS;
    app.show_covers = true;
    app_reset_preview(&app);
    app.selection_idle_frames = 6;
    cover_stats = 0;
    app_tick(&app);
    assert(app.artwork.ready && cover_stats == 1);
    for (unsigned i = 0; i < 3; i++)
        assert(!*app.shelf_paths[i]);
    /* A missing launch target can still have deliberately assigned artwork. */
    app.entries[1].kind = ITEM_MISSING;
    app_reset_preview(&app);
    artwork_load(&app);
    assert(app.artwork.ready && !strcmp(app.artwork.source, "Assigned image"));
    puts("PASS: 3000-record display cache, fresh launch checks, recovery, invalidation and staged "
         "artwork probes");
}
