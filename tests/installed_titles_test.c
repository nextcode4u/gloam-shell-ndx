/* Entire NAND fixture is synthetic metadata; never a playable title or real save. */
#include "app.h"
#include "browser.h"
#include "icons.h"
#include "installed_titles.h"
#include "layout.h"
#include "paths.h"
#include "preferences.h"
#include "system_routes.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app, restored;
static const char *watched_path;
static DIR *watched_directory;
static const InstalledCatalog *watched_catalog;
static bool fail_read, fail_after_title, fail_close, stale_errno;
static const char *watched_file_path;
static FILE *watched_file;
static bool file_metadata_error, file_read_error, file_close_error;
static size_t file_read_limit = (size_t)-1;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_fstat(int, struct stat *);
int __real_ferror(FILE *);
size_t __real_fread(void *, size_t, size_t, FILE *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    FILE *file = __real_fopen(path, mode);
    if (watched_file_path && !strcmp(path, watched_file_path) && *mode == 'r')
        watched_file = file;
    return file;
}
int __wrap_fclose(FILE *file)
{
    bool fail = file == watched_file && file_close_error;
    if (file == watched_file)
        watched_file = NULL;
    int result = __real_fclose(file);
    if (fail) {
        errno = EIO;
        return EOF;
    }
    return result;
}
int __wrap_fstat(int descriptor, struct stat *metadata)
{
    if (watched_file && descriptor == fileno(watched_file) && file_metadata_error) {
        errno = EIO;
        return -1;
    }
    return __real_fstat(descriptor, metadata);
}
int __wrap_ferror(FILE *file)
{
    return (file == watched_file && file_read_error) || __real_ferror(file);
}
size_t __wrap_fread(void *buffer, size_t size, size_t count, FILE *file)
{
    if (file == watched_file && count > file_read_limit)
        count = file_read_limit;
    return __real_fread(buffer, size, count, file);
}
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
size_t __wrap___fread_chk(void *buffer, size_t capacity, size_t size, size_t count, FILE *file)
{
    if (file == watched_file && count > file_read_limit)
        count = file_read_limit;
    return __real___fread_chk(buffer, capacity, size, count, file);
}
#endif
DIR *__real_opendir(const char *);
struct dirent *__real_readdir(DIR *);
int __real_closedir(DIR *);
DIR *__wrap_opendir(const char *path)
{
    DIR *folder = __real_opendir(path);
    if (watched_path && !strcmp(path, watched_path))
        watched_directory = folder;
    return folder;
}
struct dirent *__wrap_readdir(DIR *folder)
{
    if (folder == watched_directory &&
        (fail_read || (fail_after_title && watched_catalog && watched_catalog->count))) {
        errno = EIO;
        return NULL;
    }
    struct dirent *entry = __real_readdir(folder);
    if (folder == watched_directory && entry && stale_errno)
        errno = EIO;
    return entry;
}
int __wrap_closedir(DIR *folder)
{
    bool deny = folder == watched_directory && fail_close;
    if (folder == watched_directory)
        watched_directory = NULL;
    int result = __real_closedir(folder);
    if (deny) {
        errno = EIO;
        return -1;
    }
    return result;
}
static void be32(unsigned char *p, uint32_t n)
{
    for (int i = 0; i < 4; i++)
        p[i] = n >> (24 - i * 8);
}
static void le32(unsigned char *p, uint32_t n)
{
    for (int i = 0; i < 4; i++)
        p[i] = n >> (i * 8);
}
static void directory(const char *root, const char *relative)
{
    char p[PATH_CAP];
    assert(path_join(p, root, relative));
    assert(!mkdir(p, 0777));
}
static void make_title(const char *root, uint32_t id, const char *name, bool unsigned_tmd)
{
    char dir[PATH_CAP], path[PATH_CAP], relative[120];
    snprintf(relative, sizeof relative, "title/00030004/%08lx", (unsigned long)id);
    directory(root, relative);
    assert(path_join(dir, root, relative));
    directory(dir, "content");
    directory(dir, "data");
    assert(path_join(path, dir, "data/public.sav"));
    FILE *file = fopen(path, "wb");
    assert(file && fputs("Keep my save", file) >= 0 && !fclose(file));
    unsigned char rom[0xC40] = {0}, tmd[0x208] = {0};
    rom[0x12] = 3;
    le32(rom + 0x230, id);
    le32(rom + 0x234, 0x00030004);
    le32(rom + 0x20, 0x240);
    le32(rom + 0x2C, 4);
    le32(rom + 0x30, 0x244);
    le32(rom + 0x3C, 4);
    le32(rom + 0x68, 0x400);
    rom[0x400] = 1;
    memset(rom + 0x420, 0x11, 512);
    rom[0x622] = 31;
    for (size_t i = 0; name[i]; i++)
        rom[0x400 + 0x340 + 2 * i] = name[i];
    be32(tmd, unsigned_tmd ? 0 : 0x10001);
    be32(tmd + 0x18C, 0x30004);
    be32(tmd + 0x190, id);
    tmd[0x1DF] = 1;
    tmd[0x1E1] = 5;
    be32(tmd + 0x1E4, 7);
    tmd[0x1E9] = 5;
    be32(tmd + 0x1F0, sizeof rom);
    assert(path_join(path, dir, "content/title.tmd"));
    file = fopen(path, "wb");
    assert(file && fwrite(tmd, 1, sizeof tmd, file) == sizeof tmd && !fclose(file));
    assert(path_join(path, dir, "content/00000007.app"));
    file = fopen(path, "wb");
    assert(file && fwrite(rom, 1, sizeof rom, file) == sizeof rom && !fclose(file));
}
static void scan(InstalledCatalog *catalog, const char *root)
{
    installed_catalog_begin(catalog, root, 1);
    assert(catalog->scanning && !catalog->count && !catalog->ready);
    unsigned steps = 0;
    while (catalog->scanning) {
        installed_catalog_step(catalog);
        assert(++steps < 4098);
    }
}
static void file_read_failures(const char *root)
{
    char path[PATH_CAP];
    const char *files[] = {"title/00030004/474c4458/content/title.tmd",
                           "title/00030004/474c4458/content/00000007.app"};
    for (unsigned i = 0; i < 2; i++) {
        assert(path_join(path, root, files[i]));
        for (unsigned failure = 0; failure < 4; failure++) {
            watched_file_path = path;
            file_metadata_error = failure == 0;
            file_read_limit = failure == 1 ? 1 : (size_t)-1;
            file_read_error = failure == 2;
            file_close_error = failure == 3;
            assert(!installed_title_revalidate(&app.installed_catalog, 0));
            scan(&restored.installed_catalog, root);
            assert(restored.installed_catalog.ready && restored.installed_catalog.count == 1);
            assert(restored.installed_catalog.titles[0].title.low == 0x4B414245);
            assert(restored.installed_catalog.skipped == 2);
            watched_file_path = NULL;
            file_metadata_error = file_read_error = file_close_error = false;
            file_read_limit = (size_t)-1;
            assert(installed_title_revalidate(&app.installed_catalog, 0));
        }
    }
    scan(&restored.installed_catalog, root);
    assert(restored.installed_catalog.ready && restored.installed_catalog.count == 2);
}
static void scan_failures(const char *root)
{
    char folder[PATH_CAP];
    uint32_t selected_id = app.installed_selected_id;
    assert(path_join(folder, root, "title/00030004"));
    watched_path = folder;
    watched_catalog = &app.installed_catalog;
    app_category(&app, CATEGORY_SYSTEM);
    app_move(&app, 7);
    app_open(&app);
    assert(app.installed_open && app.entry_count == 2);
    app.installed_selected_id = 0x4B414245;
    for (unsigned failure = 0; failure < 3; failure++) {
        fail_read = failure == 0;
        fail_after_title = failure == 1;
        fail_close = failure == 2;
        installed_catalog_begin(&app.installed_catalog, root, 1);
        unsigned steps = 0;
        while (app.installed_catalog.scanning) {
            app_tick(&app);
            assert(++steps < 4098);
        }
        assert(!app.installed_catalog.ready && !app.installed_catalog.count);
        assert(!app.installed_catalog.directory && !app.entry_count);
        assert(strstr(app.installed_catalog.status, "retry"));
        assert(!installed_title_revalidate(&app.installed_catalog, 0));
        char target[PATH_CAP];
        assert(!installed_title_path(&app.installed_catalog, 0, target));
        assert(app.installed_selected_id == 0x4B414245);
        fail_read = fail_after_title = fail_close = false;
        app_back(&app);
        assert(!app.installed_open && !app.installed_catalog.attempted);
        app_move(&app, 7 - app.locations[CATEGORY_SYSTEM].selected);
        app_open(&app);
        assert(app.installed_open && app.installed_discovery_requested);
        app.installed_discovery_requested = false;
        stale_errno = true;
        installed_catalog_begin(&app.installed_catalog, root, 1);
        steps = 0;
        while (app.installed_catalog.scanning) {
            app_tick(&app);
            assert(++steps < 4098);
        }
        stale_errno = false;
        assert(app.installed_catalog.ready && app.installed_catalog.count == 2 &&
               app.entry_count == 2);
        assert(app_selected(&app)->kind == ITEM_INSTALLED_TITLE && app_selected(&app)->target == 1);
    }
    watched_path = NULL;
    watched_catalog = NULL;
    char empty[PATH_CAP];
    directory(root, "empty");
    assert(path_join(empty, root, "empty"));
    directory(empty, "title");
    directory(empty, "title/00030004");
    scan(&restored.installed_catalog, empty);
    assert(restored.installed_catalog.ready && !restored.installed_catalog.count);
    assert(strstr(restored.installed_catalog.status, "0 installed titles"));
    app_back(&app);
    app.locations[CATEGORY_SYSTEM].selected = app.locations[CATEGORY_SYSTEM].scroll = 0;
    app.installed_selected_id = selected_id;
    app_category(&app, CATEGORY_HOME);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    directory(root, "title");
    directory(root, "title/00030004");
    make_title(root, 0x4B414245, "Zulu", false);
    make_title(root, 0x474C4458, "Alpha", true);
    make_title(root, 0x4B424245, "Broken", false);
    char path[PATH_CAP];
    assert(path_join(path, root, "title/00030004/4b424245/content/title.tmd"));
    FILE *file = fopen(path, "r+b");
    assert(file && !fseek(file, 0x190, SEEK_SET) && fputc(1, file) != EOF && !fclose(file));
    app_init(&app, root, true, true);
    scan(&app.installed_catalog, root);
    assert(app.installed_catalog.ready && app.installed_catalog.count == 2 &&
           app.installed_catalog.skipped == 1);
    assert(!strcmp(app.installed_catalog.titles[0].name, "Alpha"));
    file_read_failures(root);
    scan_failures(root);
    assert(app.installed_catalog.titles[0].title.boot_content == 7);
    assert(installed_title_revalidate(&app.installed_catalog, 0));
    uint32_t request[8];
    unsigned char autoload[0x400];
    assert(system_title_request(request, &app.installed_catalog.titles[0].title));
    assert(request[2] == 0x474C4458 && request[3] == 0x30004 && request[6] == 0x17);
    assert(system_title_autoload(autoload, &app.installed_catalog.titles[0].title));
    const char *target = "nand:/title/00030004/474c4458/content/00000007.app";
    for (size_t i = 0; i <= strlen(target); i++)
        assert(autoload[0x38 + i * 2] == target[i] && !autoload[0x39 + i * 2]);
    app_category(&app, CATEGORY_SYSTEM);
    assert(app.entry_count == 8 && app.entries[7].kind == ITEM_INSTALLED_LIBRARY);
    app_move(&app, 7);
    app_open(&app);
    assert(app.installed_open && app.entry_count == 2 &&
           app.entries[0].kind == ITEM_INSTALLED_TITLE);
    app.browser_style = BROWSER_LIST;
    for (int i = 0; i < UI_ICON_IDLE_FRAMES; i++)
        app_tick(&app);
    assert(icons_for_entry(&app, 0) && icons_for_entry(&app, 0)->pixels[0] == 0x801F);
    unsigned reads = app.icon_reads;
    app.navigation_held = true;
    app_scan(&app);
    assert(icons_for_entry(&app, 0) && app.icon_reads == reads);
    app.navigation_held = false;
    app_panel(&app, PANEL_ACTIONS);
    assert(app.action_enabled[0] && app.action_enabled[2]);
    assert(!app.action_enabled[1] && !app.action_enabled[3] && !app.action_enabled[8]);
    app.panel = PANEL_NONE;
    app_move(&app, 1);
    app_open(&app);
    assert(app.system_title_requested == 6 && app.installed_selected_id == 0x4B414245);
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(restored.installed_open && restored.installed_selected_id == 0x4B414245);
    app_back(&app);
    assert(!app.installed_open && app.entries[7].kind == ITEM_INSTALLED_LIBRARY);
    installed_catalog_begin(&app.installed_catalog, root, 1);
    installed_catalog_cancel(&app.installed_catalog);
    assert(!app.installed_catalog.directory && !app.installed_catalog.scanning);
    scan(&app.installed_catalog, root);
    assert(installed_title_path(&app.installed_catalog, 0, path));
    file = fopen(path, "r+b");
    assert(file && !fseek(file, 0x230, SEEK_SET) && fputc(0, file) != EOF && !fclose(file));
    assert(!installed_title_revalidate(&app.installed_catalog, 0));
    assert(path_join(path, root, "title/00030004/474c4458/data/public.sav"));
    file = fopen(path, "rb");
    char save[32] = {0};
    assert(file && fread(save, 1, 31, file) == 12 && !fclose(file));
    assert(!strcmp(save, "Keep my save"));
    puts("PASS: bounded installed-title discovery, names/metadata identity, native requests/icons, "
         "read-error discard/retry, read-only actions, persistence and save preservation");
}
