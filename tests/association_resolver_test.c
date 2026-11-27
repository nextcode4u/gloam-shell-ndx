/* Folder-aware handlers and explicit choices, using authored executable headers. */
#include "app.h"
#include "arguments.h"
#include "associations.h"
#include "browser.h"
#include "inspection.h"
#include "item_preferences.h"
#include "launch.h"
#include "paths.h"
#include "picker.h"
#include "startup.h"
#include "top_content.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static App app;
static uint16_t top[256 * 192], bottom[256 * 192];
static bool inspecting;
static unsigned write_attempts;
static const char *watched_current, *watched_backup;
static FILE *watched_stream;
static bool fail_open, fail_metadata, fail_read, fail_close;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_fstat(int, struct stat *);
size_t __real_fread(void *, size_t, size_t, FILE *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (inspecting && strpbrk(mode, "wa+"))
        write_attempts++;
    bool watched = (watched_current && !strcmp(path, watched_current)) ||
                   (watched_backup && !strcmp(path, watched_backup));
    if (watched && *mode == 'r' && fail_open) {
        errno = EIO;
        return NULL;
    }
    FILE *file = __real_fopen(path, mode);
    if (watched && *mode == 'r')
        watched_stream = file;
    return file;
}
int __wrap_fclose(FILE *file)
{
    bool deny = file == watched_stream && fail_close;
    if (file == watched_stream)
        watched_stream = NULL;
    int result = __real_fclose(file);
    if (deny) {
        errno = EIO;
        return EOF;
    }
    return result;
}
int __wrap_fstat(int descriptor, struct stat *info)
{
    if (watched_stream && fileno(watched_stream) == descriptor && fail_metadata) {
        errno = EIO;
        return -1;
    }
    return __real_fstat(descriptor, info);
}
size_t __wrap_fread(void *buffer, size_t size, size_t count, FILE *file)
{
    return file == watched_stream && fail_read ? 0 : __real_fread(buffer, size, count, file);
}
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
size_t __wrap___fread_chk(void *buffer, size_t capacity, size_t size, size_t count, FILE *file)
{
    return file == watched_stream && fail_read
               ? 0
               : __real___fread_chk(buffer, capacity, size, count, file);
}
#endif
static void inspect_checked(const char *path)
{
    bool dirty = app.preferences_dirty;
    inspecting = true;
    inspection_prepare(&app, path);
    inspecting = false;
    assert(!write_attempts && app.preferences_dirty == dirty && !app.launch_requested);
}

static void folder(const char *root, const char *name)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name) && !mkdir(path, 0777));
}
static void text(const char *root, const char *name, const char *value)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    FILE *file = fopen(path, "w");
    assert(file && fputs(value, file) >= 0 && !fclose(file));
}
static void binary(const char *root, const char *name)
{
    unsigned char bytes[0x1200] = {0};
    memcpy(bytes + 12, "####", 4);
    for (int cpu = 0; cpu < 2; cpu++) {
        uint32_t words[] = {0x1000 + cpu * 256, cpu ? 0x037F8000 : 0x02000000,
                            cpu ? 0x037F8000 : 0x02000000, 256};
        memcpy(bytes + 0x20 + cpu * 16, words, sizeof words);
    }
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes && !fclose(file));
}
static void profile(const char *root, const char *file, const char *name, const char *extensions,
                    const char *scope, const char *application, bool argv)
{
    char path[PATH_CAP], value[4096];
    assert(path_join(path, "_nds/dsi-frontend/plugins", file));
    snprintf(value, sizeof value,
             "[plugin]\nversion=1\nname=%s\nextensions=%s\ncontent_root=%s\n"
             "application=%s\nargv=%d\nbootstrap=0\ncover=/art/%s.png\n",
             name, extensions, scope, application, argv, name);
    text(root, path, value);
}
static void remove_profile(const char *root, const char *name)
{
    char directory[PATH_CAP], path[PATH_CAP];
    assert(path_join(directory, root, "_nds/dsi-frontend/plugins"));
    assert(path_join(path, directory, name) && !remove(path));
    associations_load(&app);
}
static const Association *resolve(const char *root, const char *relative)
{
    char path[PATH_CAP];
    assert(path_join(path, root, relative));
    return association_find(&app, path);
}
static void select_file(const char *root, const char *relative)
{
    char path[PATH_CAP];
    assert(path_join(path, root, relative));
    for (int i = 0; i < app.entry_count; i++)
        if (!strcmp(app.entries[i].path, path)) {
            app.locations[app.category].selected = i;
            return;
        }
    assert(!"File absent from browser");
}
static void record_text(const char *path, const char *value)
{
    FILE *file = fopen(path, "w");
    assert(file && fputs(value, file) >= 0 && !fclose(file));
}
static void read_record_text(const char *path, char value[4096])
{
    FILE *file = fopen(path, "rb");
    assert(file);
    size_t count = fread(value, 1, 4095, file);
    assert(!ferror(file) && !fclose(file));
    value[count] = 0;
}
static void assignment_read_failures(const char *root)
{
    const char *locations[] = {"roms/Genesis/Assigned.md", "misc/Assigned.md"};
    for (unsigned location = 0; location < 2; location++) {
        char content[PATH_CAP], current[PATH_CAP], backup[PATH_CAP], name[80], key[PATH_CAP];
        text(root, locations[location], "Assigned application fixture");
        assert(path_join(content, root, locations[location]));
        ItemPreferences item = {0};
        assert(path_join(item.application, root, "apps/Reader.nds"));
        assert(item_preferences_save(root, content, &item));
        assert(item_preferences_save(root, content, &item));
        assert(path_resolve_sd(key, "sd:/", locations[location]));
        uint64_t hash = UINT64_C(14695981039346656037);
        for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
            hash ^= *p;
            hash *= UINT64_C(1099511628211);
        }
        snprintf(name, sizeof name, "items/%016" PRIx64 ".ini", hash);
        assert(path_preferences_file(current, root, name));
        strcpy(name + strlen(name) - 3, "bak");
        assert(path_preferences_file(backup, root, name));
        char original[4096], recovery[4096], actual[4096], error[160];
        read_record_text(current, original);
        read_record_text(backup, recovery);
        LaunchPlan plan;
        for (unsigned failure = 0; failure < 4; failure++) {
            watched_current = current;
            fail_open = failure == 0;
            fail_metadata = failure == 1;
            fail_read = failure == 2;
            fail_close = failure == 3;
            assert(launch_prepare(&app, content, &plan, error));
            assert(!strcmp(plan.application, item.application));
            watched_backup = backup;
            assert(!launch_prepare(&app, content, &plan, error) && strstr(error, "saved settings"));
            ItemPreferences unchanged = item;
            assert(!item_preferences_resolve(root, content, &unchanged));
            assert(!memcmp(&unchanged, &item, sizeof item));
            assert(!picker_open_handlers(&app, content) && !app.picker.open);
            inspect_checked(content);
            assert(!*app.inspection->application &&
                   strstr(app.inspection->status, "saved settings"));
            assert(strstr(app.inspection->save, "Not resolved"));
            inspection_close(&app);
            watched_current = watched_backup = NULL;
            fail_open = fail_metadata = fail_read = fail_close = false;
            read_record_text(current, actual);
            assert(!strcmp(actual, original));
            read_record_text(backup, actual);
            assert(!strcmp(actual, recovery));
        }
        record_text(current, "broken record\n");
        assert(launch_prepare(&app, content, &plan, error));
        assert(!strcmp(plan.application, item.application));
        record_text(backup, "broken recovery\n");
        assert(!launch_prepare(&app, content, &plan, error) && strstr(error, "saved settings"));
        record_text(current, original);
        record_text(backup, recovery);
        item.application[0] = 0;
        assert(item_preferences_save(root, content, &item));
        assert(item_preferences_load(root, content, &item) && !*item.application);
        if (!location) {
            assert(launch_prepare(&app, content, &plan, error));
            assert(strstr(plan.application, "Genesis.nds"));
        } else {
            assert(picker_open_handlers(&app, content) && app.picker.count == 2);
            picker_back(&app);
        }
        assert(!remove(current) && !remove(backup));
        if (!location)
            assert(launch_prepare(&app, content, &plan, error));
        else {
            assert(picker_open_handlers(&app, content));
            picker_back(&app);
        }
        assert(access(current, F_OK) && errno == ENOENT);
        assert(access(backup, F_OK) && errno == ENOENT);
        assert(!remove(content));
    }
}
static void pending_launch_replacement(const char *root)
{
    char reader[PATH_CAP], missing[PATH_CAP], content[PATH_CAP];
    assert(path_join(reader, root, "apps/Reader.nds"));
    assert(path_join(missing, root, "misc/Missing.nds"));
    assert(path_join(content, root, "misc/Choice.pending"));
    text(root, "misc/Choice.pending", "Pending launch fixture");
    profile(root, "pending-reader.ini", "Reader pending", "pending", "", "/apps/Reader.nds", true);
    profile(root, "pending-genesis.ini", "Genesis pending", "pending", "", "/apps/Genesis.nds",
            true);
    associations_load(&app);
    app_panel(&app, PANEL_NONE);
    app.launch_requested = false;
    app_launch_file(&app, reader);
    assert(app.launch_requested);
    app.startup_due = true;
    app_launch_file(&app, missing);
    assert(!app.launch_requested && !app.startup_due && app.panel == PANEL_MESSAGE);
    app_back(&app);
    app_launch_file(&app, reader);
    assert(app.launch_requested);
    app_launch_file(&app, "/outside/the/sd.nds");
    assert(!app.launch_requested && app.panel == PANEL_MESSAGE);
    app_back(&app);
    app_launch_file(&app, reader);
    assert(app.launch_requested);
    app_launch_file(&app, content);
    assert(app.picker.open && app.picker.kind == PICKER_HANDLER && !app.launch_requested);
    picker_back(&app);
    assert(!app.picker.open && !app.launch_requested);
    app_launch_file(&app, content);
    assert(app.picker.open && !app.launch_requested);
    picker_activate(&app);
    assert(app.launch_requested && !app.picker.open);
    assert(!strcmp(app.launch.content, content) && app.launch.arguments.count == 2);
    app.launch_requested = false;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    const char *directories[] = {"apps",
                                 "roms",
                                 "roms/Genesis",
                                 "media",
                                 "media/books",
                                 "media/books/deep",
                                 "misc",
                                 "_nds",
                                 "_nds/dsi-frontend",
                                 "_nds/dsi-frontend/plugins"};
    for (unsigned i = 0; i < sizeof directories / sizeof *directories; i++)
        folder(root, directories[i]);
    binary(root, "apps/Reader.nds");
    binary(root, "apps/Genesis.nds");
    binary(root, "apps/Alternate.nds");
    profile(root, "genesis.ini", "Genesis", "md,gen", "sd:/roms/Genesis", "/apps/Genesis.nds",
            true);
    profile(root, "reader.ini", "Reader", "md,markdown,fb2.zip", "sd:/media/books",
            "/apps/Reader.nds", true);
    profile(root, "invalid.ini", "Invalid", "md", "sd:/../escape", "/apps/Alternate.nds", true);
    text(root, "misc/Notes.md", "Original text fixture");
    text(root, "misc/Archive.fb2.zip", "Not parsed by the launcher");
    text(root, "misc/Other.zip", "Not a book association");
    app_init(&app, root, true, true);
    assert(app.association_count == 2);
    assignment_read_failures(root);
    const Association *reader = resolve(root, "media/books/deep/Notes.md");
    assert(reader && !strcmp(reader->name, "Reader"));
    assert(resolve(root, "MEDIA/BOOKS/Notes.MD") == reader);
    assert(!strcmp(resolve(root, "roms/Genesis/Game.md")->name, "Genesis"));
    assert(!resolve(root, "roms/Genesis-extra/Game.md"));
    assert(!resolve(root, "media/books/../misc/Notes.md"));
    assert(!resolve(root, "misc/Notes.md"));
    assert(!strcmp(resolve(root, "misc/Game.gen")->name, "Genesis"));
    assert(resolve(root, "misc/Notes.markdown") == reader);
    assert(resolve(root, "misc/Archive.fb2.zip") == reader);
    char book[PATH_CAP], cover[PATH_CAP];
    assert(path_join(book, root, "media/books/deep/Notes.md"));
    assert(path_join(cover, root, "art/Reader.png"));
    assert(!strcmp(association_cover(&app, book), cover));
    assert(!resolve(root, "misc/Other.zip"));
    /* Inspection uses the real resolver/preflight and never stages a launch. */
    char explain[PATH_CAP];
    text(root, "media/books/Explain.md", "Original inspection fixture");
    assert(path_join(explain, root, "media/books/Explain.md"));
    bool dirty = app.preferences_dirty;
    inspection_prepare(&app, explain);
    assert(app.inspection && strstr(app.inspection->reason, "Preferred folder"));
    assert(strstr(app.inspection->rule, "media/books"));
    assert(strstr(app.inspection->application, "Reader.nds"));
    assert(strstr(app.inspection->backend, "Direct") && strstr(app.inspection->backend, "argv"));
    assert(strstr(app.inspection->save, "Managed by the application"));
    assert(strstr(app.inspection->status, "Preflight passed"));
    assert(app.preferences_dirty == dirty && !app.launch_requested);
    assert(path_join(explain, root, "misc/Notes.md"));
    inspection_prepare(&app, explain);
    assert(strstr(app.inspection->reason, "Several handlers") &&
           strstr(app.inspection->backend, "Choose a handler"));
    assert(!*app.inspection->application && strstr(app.inspection->save, "Not resolved"));
    inspection_close(&app);
    assert(!app.inspection);
    char long_name[160];
    memset(long_name, 'L', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = 0;
    profile(root, "long-name.ini", long_name, "long", "", "/apps/Reader.nds", true);
    associations_load(&app);
    const Association *long_profile = resolve(root, "misc/Notes.long");
    assert(long_profile && !*long_profile->name); /* NDS filename is the display fallback. */

    profile(root, "nested.ini", "Nested", "md", "sd:/media/books/deep", "/apps/Alternate.nds",
            true);
    associations_load(&app);
    assert(!strcmp(resolve(root, "media/books/deep/Notes.md")->name, "Nested"));
    assert(!strcmp(resolve(root, "media/books/Notes.md")->name, "Reader"));
    profile(root, "nested.ini", "Nested", "md", "sd:/media/books", "/apps/Alternate.nds", true);
    associations_load(&app);
    assert(!resolve(root, "media/books/Notes.md")); /* Equal scope needs a choice. */
    remove_profile(root, "nested.ini");
    profile(root, "reader-alias.ini", "Reader alias", "md", "sd:/another-folder",
            "/apps/Reader.nds", true);
    associations_load(&app);
    char content[PATH_CAP], reader_path[PATH_CAP], error[160];
    assert(path_join(content, root, "misc/Notes.md"));
    assert(path_join(reader_path, root, "apps/Reader.nds"));
    const Association *choices[ASSOCIATION_CAP];
    assert(association_choices(&app, content, choices) ==
           2); /* Same app/route is not duplicated. */
    profile(root, "reader-alias.ini", "No argv", "md", "sd:/another-folder", "/apps/Reader.nds",
            false);
    associations_load(&app);
    assert(association_choices(&app, content, choices) == 3);
    LaunchPlan plan;
    int no_argv = -1;
    for (int i = 0; i < app.association_count; i++)
        if (!strcmp(app.associations[i].name, "No argv"))
            no_argv = i;
    assert(launch_prepare_handler(&app, content, no_argv, &plan, error));
    assert(plan.arguments.count == 1); /* Selection preserves the chosen profile's flags. */
    app.associations[no_argv].bootstrap = true;
    assert(!launch_prepare_handler(&app, content, no_argv, &plan, error));
    app.associations[no_argv].bootstrap = false;
    assert(!launch_prepare_handler(&app, content, ASSOCIATION_CAP, &plan, error));
    remove_profile(root, "reader-alias.ini");

    profile(root, "generic.ini", "Generic", "zip", "", "/apps/Alternate.nds", true);
    text(root, "_nds/dsi-frontend/plugins/block.ini",
         "[plugin]\nextensions=blocked.zip\ncontent_root=sd:/media/books\ndisabled=1\n");
    associations_load(&app);
    assert(!resolve(root, "media/books/Example.blocked.zip"));
    assert(!strcmp(resolve(root, "misc/Example.blocked.zip")->name, "Generic"));
    remove_profile(root, "block.ini");
    remove_profile(root, "generic.ini");

    app_category(&app, CATEGORY_FILES);
    assert(path_join(app.locations[CATEGORY_FILES].path, root, "misc"));
    app.filter_recognized = true;
    app_scan(&app);
    assert(app.entry_count == 2); /* Ambiguous .md and compound suffix remain visible. */
    select_file(root, "misc/Notes.md");
    Location location = app.locations[app.category];
    assert(!launch_prepare(&app, content, &plan, error) && strstr(error, "Multiple"));
    app_open(&app);
    assert(app.picker.open && app.picker.kind == PICKER_HANDLER && app.picker.count == 2);
    assert(!app.launch_requested && !app.picker.directory && !app.picker.visited);
    picker_render(&app, top, bottom);
    picker_back(&app);
    assert(!app.picker.open && !memcmp(&location, &app.locations[app.category], sizeof location));
    ItemPreferences item;
    assert(!item_preferences_load(root, content, &item));
    app_open(&app);
    int selected = -1;
    for (int i = 0; i < app.picker.count; i++)
        if (!strcmp(app.picker.entries[i].name, "Reader"))
            selected = i;
    assert(selected >= 0);
    picker_move(&app, selected);
    picker_activate(&app);
    assert(!app.picker.open && app.launch_requested);
    assert(!strcmp(app.launch.application, reader_path));
    assert(app.launch.arguments.count == 2 && app.launch.route == LAUNCH_DIRECT);
    assert(!strcmp(arguments_get(&app.launch.arguments, 1), content));
    assert(!item_preferences_load(root, content, &item)); /* One launch does not save defaults. */
    app.launch_requested = false;
    path_copy(app.dashboard.recent[0].path, content, PATH_CAP);
    app.startup_countdown = 1;
    app.startup_cancelled = app.startup_due = false;
    startup_tick(&app);
    assert(app.picker.open && !app.launch_requested && !app.startup_due);
    picker_back(&app);
    startup_tick(&app);
    assert(!app.picker.open && !app.launch_requested); /* No repeated boot prompt/countdown. */
    assert(association_set_default(&app, "md", reader_path));
    assert(!strcmp(association_cover(&app, book), cover));
    assert(!association_cover(&app, content));
    app_open(&app);
    assert(app.launch_requested && !app.picker.open);
    app.launch_requested = false;
    assert(association_disable(&app, "md") && !association_choices(&app, content, choices));
    memset(&item, 0, sizeof item);
    path_copy(item.application, reader_path, sizeof item.application);
    assert(item_preferences_save(root, content, &item));
    app_open(&app);
    assert(app.launch_requested && !app.picker.open); /* Per-file choice still wins. */
    app.launch_requested = false;
    item.application[0] = 0;
    assert(item_preferences_save(root, content, &item));
    assert(association_clear_default(&app, "md"));

    profile(root, "genesis.ini", "Missing", "md,gen", "sd:/roms/Genesis", "/apps/Missing.nds",
            true);
    associations_load(&app);
    app_open(&app);
    selected = -1;
    for (int i = 0; i < app.picker.count; i++)
        if (!strcmp(app.picker.entries[i].name, "Missing"))
            selected = i;
    assert(selected >= 0);
    picker_move(&app, selected);
    picker_activate(&app);
    assert(app.picker.open && *app.picker.status && !app.launch_requested);
    picker_render(&app, top, bottom);
    picker_back(&app);
    assert(!memcmp(&location, &app.locations[app.category], sizeof location));
    text(root, "misc/Explicit.ARGV", "sd:/apps/Reader.nds sd:/misc/Notes.md\n");
    app_scan(&app);
    select_file(root, "misc/Explicit.ARGV");
    app_open(&app);
    assert(app.launch_requested && !app.picker.open && app.launch.arguments.count == 2);
    app.launch_requested = false;
    app_category(&app, CATEGORY_APPS);
    select_file(root, "apps/Reader.nds");
    app_open(&app);
    assert(app.launch_requested && !app.picker.open && app.launch.arguments.count == 1);
    /* Built-in NDS/ARGV routes ignore extension handlers; diagnostics must too. */
    app.launch_requested = false;
    profile(root, "native.ini", "Misleading", "nds", "", "/apps/Alternate.nds", true);
    associations_load(&app);
    assert(launch_prepare(&app, reader_path, &plan, error));
    assert(plan.route == LAUNCH_DIRECT && plan.arguments.count == 1);
    inspect_checked(reader_path);
    assert(!strcmp(app.inspection->application, plan.application));
    assert(!strcmp(app.inspection->backend, "Direct homebrew loader"));
    app.top_content = TOP_CONTENT_CONTEXT;
    app.detail_tab = 0;
    app_reset_preview(&app);
    app.selection_idle_frames = 7;
    for (unsigned i = 0; i < 4; i++)
        top_content_tick(&app);
    assert(!strcmp(app.top.handler, "DS/DSi title"));
    profile(root, "native-second.ini", "Also ignored", "nds", "", "/apps/Genesis.nds", true);
    associations_load(&app);
    inspect_checked(reader_path);
    assert(!strcmp(app.inspection->backend, "Direct homebrew loader"));

    memset(&item, 0, sizeof item);
    assert(path_join(item.application, root, "apps/Alternate.nds"));
    assert(item_preferences_save(root, reader_path, &item));
    assert(launch_prepare(&app, reader_path, &plan, error) && plan.arguments.count == 2);
    inspect_checked(reader_path);
    assert(!strcmp(app.inspection->application, item.application));
    assert(strstr(app.inspection->backend, "Direct") && strstr(app.inspection->backend, "argv"));
    item.application[0] = 0;
    assert(item_preferences_save(root, reader_path, &item));

    profile(root, "shortcut.ini", "Ignored shortcut handler", "argv", "", "/apps/Alternate.nds",
            true);
    associations_load(&app);
    assert(path_join(explain, root, "misc/Explicit.ARGV"));
    assert(launch_prepare(&app, explain, &plan, error) && plan.arguments.count == 2);
    inspect_checked(explain);
    assert(!strcmp(app.inspection->application, plan.application));
    assert(strstr(app.inspection->backend, "Direct") && strstr(app.inspection->backend, "argv"));
    profile(root, "shortcut-second.ini", "Also ignored", "argv", "", "/apps/Genesis.nds", true);
    associations_load(&app);
    inspect_checked(explain);
    assert(strstr(app.inspection->backend, "Direct"));
    text(root, "misc/Explicit.ARGV", "sd:/apps/Reader.nds\n");
    assert(launch_prepare(&app, explain, &plan, error) && plan.arguments.count == 1);
    inspect_checked(explain);
    assert(!strcmp(app.inspection->backend, "Direct homebrew loader"));
    text(root, "misc/Explicit.ARGV", "sd:/apps/Absent.nds\n");
    assert(!launch_prepare(&app, explain, &plan, error));
    inspect_checked(explain);
    assert(!strstr(app.inspection->status, "Preflight passed"));
    /* HB arguments live in the backend plan, not its own argv list. */
    binary(root, "_nds/nds-bootstrap-hb-release.nds");
    text(root, "misc/Content.hbtest", "Synthetic homebrew content");
    text(root, "_nds/dsi-frontend/plugins/backend.ini",
         "[plugin]\nversion=1\nextensions=hbtest\napplication=/apps/Reader.nds\n"
         "argv=1\nbootstrap=1\n");
    associations_load(&app);
    assert(path_join(explain, root, "misc/Content.hbtest"));
    assert(launch_prepare(&app, explain, &plan, error));
    assert(plan.route == LAUNCH_BOOTSTRAP_HB && plan.arguments.count == 1 &&
           *plan.homebrew_argument);
    inspect_checked(explain);
    assert(!strcmp(app.inspection->application, reader_path));
    assert(!strcmp(app.inspection->backend, "nds-bootstrap / homebrew + argv"));
    text(root, "_nds/dsi-frontend/plugins/backend.ini",
         "[plugin]\nversion=1\nextensions=hbtest\napplication=/apps/Reader.nds\n"
         "argv=0\nbootstrap=1\n");
    associations_load(&app);
    assert(launch_prepare(&app, explain, &plan, error) && !*plan.homebrew_argument);
    inspect_checked(explain);
    assert(!strcmp(app.inspection->backend, "nds-bootstrap / homebrew"));
    inspection_close(&app);

    pending_launch_replacement(root);
    puts(
        "PASS: folder/descendant/boundary rules, shared suffix chooser, route flags, cancellation, "
        "missing apps, supported filter, and personal/per-file precedence");
}
