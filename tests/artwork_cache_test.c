#include "app.h"
#include "artwork.h"
#include "artwork_disk_cache.h"
#include "associations.h"
#include "browser.h"
#include "paths.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static App app;
static Artwork image;
static void copy_file(const char *source, const char *destination)
{
    FILE *in = fopen(source, "rb"), *out = fopen(destination, "wb");
    assert(in && out);
    char buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1, sizeof buffer, in)))
        assert(fwrite(buffer, 1, count, out) == count);
    assert(!ferror(in));
    assert(!fclose(in) && !fclose(out));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    char path[PATH_CAP], first[PATH_CAP];
    artwork_cache_clear();
    for (int i = 0; i < 5; i++) {
        char name[24];
        snprintf(name, sizeof name, "cover%d.jpg", i);
        assert(path_join(path, root, name));
        copy_file("tests/fixtures/cover.jpg", path);
        if (i == 0)
            path_copy(first, path, PATH_CAP);
        assert(artwork_cached_read(&image, path, false));
        unsigned reads = artwork_cache_decodes();
        assert(artwork_cached_read(&image, path, false) && artwork_cache_decodes() == reads);
    }
    assert(artwork_cache_decodes() == 5);
    assert(artwork_cached_read(&image, first, false) &&
           artwork_cache_decodes() == 6); /* LRU eviction. */
    FILE *file = fopen(first, "wb");
    assert(file && fputs("invalid image", file) >= 0 && !fclose(file));
    assert(!artwork_cached_read(&image, first, false) && artwork_cache_decodes() == 7);
    assert(!artwork_cached_read(&image, first, false) &&
           artwork_cache_decodes() == 7); /* Negative cache. */
    copy_file("tests/fixtures/cover.jpg", first);
    assert(artwork_cached_read(&image, first, false) && artwork_cache_decodes() == 8);
    const char *dirs[] = {"roms", "_nds", "_nds/dsi-frontend", "_nds/dsi-frontend/plugins",
                          "_nds/dsi-frontend/cache"};
    for (unsigned i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        assert(path_join(path, root, dirs[i]));
        assert(!mkdir(path, 0777));
    }
    assert(path_join(path, root, "_nds/dsi-frontend/plugins/demo.ini"));
    file = fopen(path, "w");
    assert(file);
    fputs("[plugin]\nversion=1\nextensions=demo\napplication=/apps/Player.nds\ncover=/cover0.jpg\n",
          file);
    assert(!fclose(file));
    assert(path_join(path, root, "roms/Book.demo"));
    file = fopen(path, "w");
    assert(file && !fclose(file));
    app_init(&app, root, true, true);
    app_category(&app, CATEGORY_GAMES);
    const Association *profile = association_find(&app, path);
    assert(profile && *profile->cover);
    artwork_load(&app);
    assert(app.artwork.ready && !strcmp(app.artwork.source, "Association cover"));
    app.user_association_count = 1;
    path_copy(app.user_associations[0].extensions, "demo",
              sizeof app.user_associations[0].extensions);
    path_copy(app.user_associations[0].application, "sd:/apps/Other.nds", PATH_CAP);
    artwork_load(&app);
    assert(app.artwork.ready && !strcmp(app.artwork.source, "Association cover"));
    app.user_associations[0].disabled = true;
    assert(association_cover(&app, path) == NULL);
    app.user_associations[0].disabled = false;
    char sidecar[PATH_CAP];
    assert(snprintf(sidecar, sizeof sidecar, "%s.jpg", path) < (int)sizeof sidecar);
    copy_file("tests/fixtures/cover.jpg", sidecar);
    artwork_load(&app);
    assert(!strcmp(app.artwork.source, "Sidecar JPEG"));
    browser_refresh(&app);
    assert(artwork_cache_decodes() == 0);
    /* A RAM reset must reuse prepared pixels without another image decode. */
    artwork_disk_cache_configure(root, true);
    assert(artwork_disk_cache_clear());
    artwork_cache_clear();
    assert(artwork_cached_read(&image, first, false));
    Artwork expected = image;
    assert(artwork_cache_decodes() == 1 && artwork_cache_disk_hits() == 0);
    artwork_cache_clear();
    memset(&image, 0, sizeof image);
    assert(artwork_cached_read(&image, first, false));
    assert(artwork_cache_decodes() == 0 && artwork_cache_disk_hits() == 1);
    assert(image.width == expected.width && image.height == expected.height &&
           !memcmp(image.pixels, expected.pixels,
                   (size_t)image.width * image.height * sizeof *image.pixels));
    char cache_dir[PATH_CAP], cache_file[PATH_CAP] = "";
    assert(path_join(cache_dir, root, "_nds/dsi-frontend/cache/artwork"));
    DIR *directory = opendir(cache_dir);
    assert(directory);
    struct dirent *record;
    while ((record = readdir(directory)))
        if (strstr(record->d_name, ".bin"))
            assert(path_join(cache_file, cache_dir, record->d_name));
    assert(!closedir(directory) && *cache_file);
    /* Corrupted prepared pixels must fall back to the original image. */
    file = fopen(cache_file, "r+b");
    assert(file && !fseek(file, 56 + (long)strlen(first), SEEK_SET));
    int byte = fgetc(file);
    assert(byte != EOF && !fseek(file, -1, SEEK_CUR) && fputc(byte ^ 1, file) != EOF &&
           !fclose(file));
    artwork_cache_clear();
    assert(artwork_cached_read(&image, first, false));
    assert(artwork_cache_decodes() == 1 && artwork_cache_disk_hits() == 0);
    /* A changed source cannot be rescued by its older valid thumbnail. */
    file = fopen(first, "wb");
    assert(file && fputs("changed source", file) >= 0 && !fclose(file));
    artwork_cache_clear();
    assert(!artwork_cached_read(&image, first, false) && artwork_cache_disk_hits() == 0);
    copy_file("tests/fixtures/cover.jpg", first);
    artwork_cache_clear();
    assert(!artwork_cached_read(&image, first, true)); /* Different embedded mode. */
    assert(artwork_cache_disk_hits() == 0);
    assert(artwork_disk_cache_clear());
    char unrelated[PATH_CAP];
    assert(path_join(unrelated, cache_dir, "keep.txt"));
    file = fopen(unrelated, "w");
    assert(file && fputs("Not a thumbnail", file) >= 0 && !fclose(file));
    /* A failed optional cache writer must not prevent artwork display. */
    char blocked[PATH_CAP];
    path_copy(blocked, cache_file, sizeof blocked);
    strcpy(strrchr(blocked, '.'), ".tmp");
    assert(!mkdir(blocked, 0777));
    artwork_cache_clear();
    assert(artwork_cached_read(&image, first, false));
    assert(artwork_cache_decodes() == 1 && !rmdir(blocked));
    struct stat source_status;
    assert(!stat(first, &source_status));
    for (int i = 0; i < 70; i++) {
        char identity[PATH_CAP];
        snprintf(identity, sizeof identity, "%s/source-%d.jpg", root, i);
        artwork_disk_cache_write(&image, identity, false, &source_status);
    }
    unsigned thumbnails = 0;
    directory = opendir(cache_dir);
    assert(directory);
    while ((record = readdir(directory)))
        if (strstr(record->d_name, ".bin"))
            thumbnails++;
    assert(!closedir(directory) && thumbnails > 0 && thumbnails <= 64);
    assert(artwork_disk_cache_clear());
    file = fopen(unrelated, "r");
    assert(file && !fclose(file)); /* Scoped clear preserves unknown files. */
    artwork_disk_cache_configure(NULL, false);
    artwork_cache_clear();
    assert(artwork_cached_read(&image, first, false) && artwork_cache_disk_hits() == 0);
    /* Shelf resolves neighbors without replacing the selected metadata. */
    app.browser_style = BROWSER_SHELF;
    app.show_covers = true;
    app.entry_count = 3;
    app.locations[app.category].selected = app.locations[app.category].scroll = 0;
    for (int i = 0; i < 3; i++) {
        app.entries[i].kind = ITEM_FILE;
        snprintf(app.entries[i].path, PATH_CAP, "%s/roms/Shelf%d.demo", root, i);
        snprintf(path, sizeof path, "%s/roms/Shelf%d.demo.jpg", root, i);
        copy_file("tests/fixtures/cover.jpg", path);
    }
    app_reset_preview(&app);
    app.selection_idle_frames = 8;
    artwork_load(&app);
    strcpy(app.title, "Selected title");
    for (int i = 0; i < 3; i++)
        artwork_shelf_tick(&app);
    for (int i = 0; i < 3; i++)
        assert(app.shelf_resolved[i] && app.shelf_artwork[i].ready);
    assert(!strcmp(app.title, "Selected title"));
    unsigned shelf_reads = app.artwork_reads;
    artwork_shelf_tick(&app);
    assert(app.artwork_reads == shelf_reads);
    app_reset_preview(&app);
    assert(!app.shelf_resolved[0] && !app.shelf_artwork[2].ready);
    puts("PASS: bounded prepared artwork LRU, negative cache, source invalidation, refresh and "
         "association fallback priority");
}
