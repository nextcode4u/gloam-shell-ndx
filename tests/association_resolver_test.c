/* Folder-aware handlers and explicit choices, using authored executable headers. */
#include "app.h"
#include "arguments.h"
#include "associations.h"
#include "browser.h"
#include "item_preferences.h"
#include "launch.h"
#include "paths.h"
#include "picker.h"
#include "startup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app;
static uint16_t top[256 * 192], bottom[256 * 192];
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
    puts(
        "PASS: folder/descendant/boundary rules, shared suffix chooser, route flags, cancellation, "
        "missing apps, supported filter, and personal/per-file precedence");
}
