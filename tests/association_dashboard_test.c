#include "app.h"
#include "appearance.h"
#include "association_editor.h"
#include "associations.h"
#include "browser.h"
#include "dashboard.h"
#include "diagnostics.h"
#include "launch.h"
#include "paths.h"
#include "picker.h"
#include "preferences.h"
#include "renderer.h"
#include "settings.h"
#include "system_routes.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *watched_profile;
static FILE *failed_profile;
static unsigned profile_failure, profile_lines;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_fstat(int, struct stat *);
size_t __real_fread(void *, size_t, size_t, FILE *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    FILE *file = __real_fopen(path, mode);
    if (watched_profile && *mode == 'r' && !strcmp(path, watched_profile)) {
        failed_profile = file;
        profile_lines = 0;
    }
    return file;
}
char *__real_fgets(char *, int, FILE *);
char *__wrap_fgets(char *data, int size, FILE *file)
{
    if (file == failed_profile && profile_failure == 1 && profile_lines++ >= 4)
        return NULL;
    return __real_fgets(data, size, file);
}
#ifdef __GLIBC__
char *__real___fgets_chk(char *, size_t, int, FILE *);
char *__wrap___fgets_chk(char *data, size_t capacity, int size, FILE *file)
{
    if (file == failed_profile && profile_failure == 1 && profile_lines++ >= 4)
        return NULL;
    return __real___fgets_chk(data, capacity, size, file);
}
#endif
int __wrap_fclose(FILE *file)
{
    bool fail = file == failed_profile && profile_failure == 2;
    if (file == failed_profile)
        failed_profile = NULL;
    int result = __real_fclose(file);
    if (fail) {
        errno = EIO;
        return EOF;
    }
    return result;
}
int __wrap_fstat(int descriptor, struct stat *status)
{
    if (failed_profile && descriptor == fileno(failed_profile) && profile_failure == 0) {
        errno = EIO;
        return -1;
    }
    return __real_fstat(descriptor, status);
}
size_t __wrap_fread(void *data, size_t size, size_t count, FILE *file)
{
    return file == failed_profile && profile_failure == 1 ? 0
                                                          : __real_fread(data, size, count, file);
}
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
size_t __wrap___fread_chk(void *data, size_t capacity, size_t size, size_t count, FILE *file)
{
    return file == failed_profile && profile_failure == 1
               ? 0
               : __real___fread_chk(data, capacity, size, count, file);
}
#endif

static App app, restored;
static uint16_t top[256 * 192], bottom[256 * 192];
static void directory(const char *root, const char *name)
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
static void check_profile_reads(const char *root)
{
    char user[PATH_CAP];
    assert(path_preferences_file(user, root, "plugins/user"));
    assert(!mkdir(user, 0777) || errno == EEXIST);
    for (unsigned personal = 0; personal < 2; personal++) {
        const char *name = personal ? "_nds/dsi-frontend/plugins/user/zzz.ini"
                                    : "_nds/dsi-frontend/plugins/zzz.ini";
        const char *profile = "[plugin]\nversion=1\nextensions=zzz\n"
                              "application=/apps/Probe.nds\nargv=0\nbootstrap=1\nhbargv=1\n";
        text(root, name, profile);
        char path[PATH_CAP];
        assert(path_join(path, root, name));
        for (profile_failure = 0; profile_failure < 3; profile_failure++) {
            watched_profile = path;
            associations_load(&app);
            assert(!association_find(&app, "sd:/roms/Example.zzz"));
            watched_profile = NULL;
            associations_load(&app);
            const Association *loaded = association_find(&app, "sd:/roms/Example.zzz");
            assert(loaded && !loaded->use_argv && loaded->bootstrap && loaded->hbargv);
        }
        const char binary_profile[] = "[plugin]\napplication=/apps/Probe.nds\n"
                                      "extensions=zzz\nargv=1\0bootstrap=1\n";
        FILE *file = fopen(path, "wb");
        assert(file &&
               fwrite(binary_profile, 1, sizeof binary_profile - 1, file) ==
                   sizeof binary_profile - 1 &&
               !fclose(file));
        associations_load(&app);
        assert(!association_find(&app, "sd:/roms/Example.zzz"));
        assert(!remove(path));
    }
    associations_load(&app);
}
static void binary(const char *root)
{
    unsigned char data[0x1200] = {0};
    memcpy(data + 12, "####", 4);
    for (int cpu = 0; cpu < 2; cpu++) {
        uint32_t words[] = {0x1000 + cpu * 256, cpu ? 0x037F8000 : 0x02000000,
                            cpu ? 0x037F8000 : 0x02000000, 256};
        memcpy(data + 0x20 + cpu * 16, words, sizeof words);
    }
    char path[PATH_CAP];
    assert(path_join(path, root, "apps/Probe.nds"));
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(data, 1, sizeof data, file) == sizeof data && !fclose(file));
}
static void check_dashboard_recovery(const char *root)
{
    app_init(&restored, root, true, true);
    text(root, "_nds/dsi-frontend/dashboard.bak",
         "version=1\norder=3,2,1,0\nvisible=1,0,1,1\nlimits=2,3,4,5\n");
    const char *damaged[] = {
        "version=1\n",
        "version=1\norder=0,1,2,3\n",
        "version=1\norder=4294967296,1,2,3\nvisible=1,1,1,1\n",
        "version=1\norder=-4294967296,1,2,3\nvisible=1,1,1,1\n",
        "version=1\norder=0,1,2,3\nvisible=4294967297,1,1,1\n",
        "version=1\norder=0,1,2,3\nvisible=1,1,1,1\nlimits=4294967296,0,0,0\n",
        "version=1\norder=0,1,2,3junk\nvisible=1,1,1,1\n",
        "version=1\norder=0,1,2,3\nvisible=1,1,1\n",
        "version=1\norder=0,1,2,3\nvisible=1,1,1,1junk\n",
        "version=1\norder=0,1,2,3\nvisible=1,1,1,1\nlimits=0,0,0,0junk\n",
        "version=1\norder=0,1,2,3\nvisible=1,1,1,1\nlimits=999999999999999999999,0,0,0\n",
        "version=1\norder=0,0,2,3\nvisible=1,1,1,1\n",
        "version=1\nversion=1\norder=0,1,2,3\nvisible=1,1,1,1\n",
        "version=1\norder=0,1,2,3\norder=0,1,2,3\nvisible=1,1,1,1\n",
        "version=1\norder=0,1,2,3\nvisible=1,1,1,1\nvisible=1,1,1,1\n",
    };
    const bool enabled[] = {true, false, true, true};
    for (unsigned n = 0; n < sizeof damaged / sizeof *damaged; n++) {
        text(root, "_nds/dsi-frontend/dashboard.ini", damaged[n]);
        dashboard_load(&restored);
        for (int i = 0; i < DASH_SECTION_COUNT; i++) {
            assert(restored.dashboard.config.order[i] == 3 - i);
            assert(restored.dashboard.config.enabled[i] == enabled[i]);
            assert(restored.dashboard.config.limit[i] == i + 2);
        }
    }
    char config_path[PATH_CAP];
    assert(path_preferences_file(config_path, root, "dashboard.ini"));
    const char embedded_nul[] = "version=1\norder=0,1,2,3\nvisible=1,1,1,1\0ignored";
    FILE *damaged_file = fopen(config_path, "wb");
    assert(damaged_file &&
           fwrite(embedded_nul, 1, sizeof embedded_nul - 1, damaged_file) ==
               sizeof embedded_nul - 1 &&
           !fclose(damaged_file));
    dashboard_load(&restored);
    assert(restored.dashboard.config.order[0] == DASH_RECENT);
    /* Limits are optional in legacy files; unknown metadata stays ignorable. */
    text(root, "_nds/dsi-frontend/dashboard.ini",
         "version=1\r\norder=0,1,2,3\r\nvisible=0,1,0,1\r\nfuture_metadata=example");
    dashboard_load(&restored);
    for (int i = 0; i < DASH_SECTION_COUNT; i++) {
        assert(restored.dashboard.config.order[i] == i);
        assert(restored.dashboard.config.enabled[i] == (i % 2 != 0));
        assert(!restored.dashboard.config.limit[i]);
    }
}
static void check_recent_recovery(const char *root)
{
    app_init(&restored, root, true, true);
    text(root, "_nds/dsi-frontend/recent.bak", "version=1\nrequest=1234,sd:/apps/Probe.nds\n");
    const char *damaged[] = {
        "version=1\nrequest=4294967296,sd:/roms/Content.xyz\n",
        "version=1\nrequest=-1,sd:/roms/Content.xyz\n",
        "version=1\nrequest=9999999999999999999999,sd:/roms/Content.xyz\n",
        "version=1\nrequest=bad,sd:/roms/Content.xyz\n",
        "version=1\nrequest=1 sd:/roms/Content.xyz\n",
        "version=1\nrequest=1,\n",
        "version=1\nrequest=1,sd:/../outside\n",
        "version=1\nrequest=1,fat:/outside\n",
        "version=1\nversion=1\nrequest=1,sd:/roms/Content.xyz\n",
        "version=2\nversion=1\nrequest=1,sd:/roms/Content.xyz\n",
    };
    char expected[PATH_CAP];
    assert(path_join(expected, root, "apps/Probe.nds"));
    for (unsigned n = 0; n < sizeof damaged / sizeof *damaged; n++) {
        text(root, "_nds/dsi-frontend/recent.ini", damaged[n]);
        dashboard_load(&restored);
        assert(restored.dashboard.recent_count == 1);
        assert(!strcmp(restored.dashboard.recent[0].path, expected));
        assert(restored.dashboard.recent[0].timestamp == 1234);
    }
    char path[PATH_CAP];
    assert(path_preferences_file(path, root, "recent.ini"));
    const char nul[] = "version=1\nrequest=1,sd:/roms/Content.xyz\0ignored";
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(nul, 1, sizeof nul - 1, file) == sizeof nul - 1 && !fclose(file));
    dashboard_load(&restored);
    assert(restored.dashboard.recent_count == 1 &&
           !strcmp(restored.dashboard.recent[0].path, expected));
    /* Clearing history is valid and must not revive an older backup. */
    text(root, "_nds/dsi-frontend/recent.ini", "version=1\r\n");
    dashboard_load(&restored);
    assert(!restored.dashboard.recent_count);
    text(root, "_nds/dsi-frontend/recent.ini",
         "version=1\r\nrequest=4294967295,sd:/roms/Content.xyz\r\n"
         "request=0,sd:/roms/Name, Part 2.xyz");
    dashboard_load(&restored);
    assert(restored.dashboard.recent_count == 2);
    assert(restored.dashboard.recent[0].timestamp == UINT32_MAX);
    assert(!restored.dashboard.recent[1].timestamp);
    assert(strstr(restored.dashboard.recent[1].path, "Name, Part 2.xyz"));
    /* Older long lists retain their first bounded set, in file order. */
    file = fopen(path, "wb");
    assert(file && fputs("version=1\n", file) >= 0);
    for (int i = 0; i <= RECENT_CAP; i++)
        assert(fprintf(file, "request=%d,sd:/roms/Entry%d.nds\r\n", i, i) > 0);
    assert(fputs("future_metadata=example", file) >= 0 && !fclose(file));
    dashboard_load(&restored);
    assert(restored.dashboard.recent_count == RECENT_CAP);
    assert(!restored.dashboard.recent[0].timestamp);
    assert(restored.dashboard.recent[RECENT_CAP - 1].timestamp == RECENT_CAP - 1);
}
static void check_shortcut_merge(const char *root)
{
    app_init(&restored, root, true, true);
    char missing[PATH_CAP], target[PATH_CAP], other[PATH_CAP], blocked[PATH_CAP];
    assert(path_join(missing, root, "roms/MergedMissing.xyz"));
    assert(path_join(target, root, "roms/Content.xyz"));
    assert(path_join(other, root, "apps/Probe.nds"));
    restored.favorite_count = 3;
    path_copy(restored.favorites[0], missing, PATH_CAP);
    path_copy(restored.favorites[1], target, PATH_CAP);
    path_copy(restored.favorites[2], other, PATH_CAP);
    assert(app_save(&restored));
    text(root, "_nds/dsi-frontend/recent.ini",
         "version=1\nrequest=300,sd:/roms/MergedMissing.xyz\n"
         "request=200,sd:/apps/Probe.nds\nrequest=100,sd:/roms/Content.xyz\n");
    dashboard_load(&restored);
    assert(restored.dashboard.recent_count == 3);
    /* Fail the second file write: both in-memory and reloaded references survive. */
    assert(path_preferences_file(blocked, root, "recent.tmp") && !mkdir(blocked, 0777));
    assert(!dashboard_relink(&restored, missing, target));
    assert(restored.favorite_count == 3 && !strcmp(restored.favorites[0], missing) &&
           !strcmp(restored.favorites[1], target) && !strcmp(restored.favorites[2], other));
    assert(restored.dashboard.recent_count == 3 &&
           !strcmp(restored.dashboard.recent[0].path, missing) &&
           restored.dashboard.recent[0].timestamp == 300 &&
           !strcmp(restored.dashboard.recent[1].path, other) &&
           restored.dashboard.recent[1].timestamp == 200 &&
           !strcmp(restored.dashboard.recent[2].path, target) &&
           restored.dashboard.recent[2].timestamp == 100);
    app_init(&app, root, true, true);
    assert(app.favorite_count == 3 && !strcmp(app.favorites[0], missing));
    assert(app.dashboard.recent_count == 3 && !strcmp(app.dashboard.recent[0].path, missing));
    assert(!rmdir(blocked));
    assert(dashboard_relink(&restored, missing, target));
    assert(restored.favorite_count == 2 && !strcmp(restored.favorites[0], target) &&
           !strcmp(restored.favorites[1], other));
    assert(restored.dashboard.recent_count == 2);
    assert(!strcmp(restored.dashboard.recent[0].path, target) &&
           restored.dashboard.recent[0].timestamp == 300);
    assert(!strcmp(restored.dashboard.recent[1].path, other));
    app_init(&app, root, true, true);
    assert(app.favorite_count == 2 && app.dashboard.recent_count == 2);
    /* When the destination was already newest, keep its original timestamp. */
    restored.favorite_count = 2;
    path_copy(restored.favorites[0], target, PATH_CAP);
    path_copy(restored.favorites[1], missing, PATH_CAP);
    restored.dashboard.recent_count = 2;
    path_copy(restored.dashboard.recent[0].path, target, PATH_CAP);
    restored.dashboard.recent[0].timestamp = 500;
    path_copy(restored.dashboard.recent[1].path, missing, PATH_CAP);
    restored.dashboard.recent[1].timestamp = 100;
    assert(dashboard_relink(&restored, missing, target));
    assert(restored.favorite_count == 1 && restored.dashboard.recent_count == 1 &&
           restored.dashboard.recent[0].timestamp == 500);
    /* A stored target string remains stable while references are compacted. */
    restored.favorite_count = 3;
    path_copy(restored.favorites[0], missing, PATH_CAP);
    path_copy(restored.favorites[1], target, PATH_CAP);
    path_copy(restored.favorites[2], other, PATH_CAP);
    restored.dashboard.recent_count = 3;
    path_copy(restored.dashboard.recent[0].path, missing, PATH_CAP);
    restored.dashboard.recent[0].timestamp = 300;
    path_copy(restored.dashboard.recent[1].path, other, PATH_CAP);
    restored.dashboard.recent[1].timestamp = 200;
    path_copy(restored.dashboard.recent[2].path, target, PATH_CAP);
    restored.dashboard.recent[2].timestamp = 100;
    assert(dashboard_relink(&restored, missing, restored.favorites[1]));
    assert(restored.favorite_count == 2 && !strcmp(restored.favorites[1], other));
    assert(restored.dashboard.recent_count == 2 &&
           !strcmp(restored.dashboard.recent[0].path, target) &&
           restored.dashboard.recent[0].timestamp == 300);
    /* Walk the actual Locate picker when its destination is already pinned. */
    restored.favorite_count = 3;
    path_copy(restored.favorites[0], target, PATH_CAP);
    path_copy(restored.favorites[1], missing, PATH_CAP);
    path_copy(restored.favorites[2], other, PATH_CAP);
    text(root, "_nds/dsi-frontend/dashboard.ini", "version=1\norder=0,1,2,3\nvisible=1,1,1,0\n");
    dashboard_load(&restored);
    app_category(&restored, CATEGORY_HOME);
    int selected = -1;
    for (int i = 0; i < restored.entry_count; i++)
        if (!strcmp(restored.entries[i].path, missing))
            selected = i;
    assert(selected >= 0);
    restored.locations[CATEGORY_HOME].selected = selected;
    app_open(&restored);
    assert(restored.picker.open && restored.picker.kind == PICKER_REPAIR);
    char roms[PATH_CAP];
    assert(path_join(roms, root, "roms"));
    selected = -1;
    for (int i = 0; i < restored.picker.count; i++)
        if (!strcmp(restored.picker.entries[i].path, roms))
            selected = i;
    assert(selected >= 0);
    restored.picker.selected = selected;
    picker_activate(&restored);
    selected = -1;
    for (int i = 0; i < restored.picker.count; i++)
        if (!strcmp(restored.picker.entries[i].path, target))
            selected = i;
    assert(selected >= 0);
    restored.picker.selected = selected;
    picker_activate(&restored);
    assert(!restored.picker.open && !restored.launch_requested);
    assert(restored.favorite_count == 2 && app_selected(&restored) &&
           !strcmp(app_selected(&restored)->path, target));
    assert(restored.preferences_dirty);
    app_back(&restored);
    assert(app_save(&restored));
    app_init(&app, root, true, true);
    assert(app.category == CATEGORY_HOME && app_selected(&app) &&
           !strcmp(app_selected(&app)->path, target));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    directory(root, "apps");
    directory(root, "roms");
    directory(root, "_nds");
    directory(root, "_nds/dsi-frontend");
    directory(root, "_nds/dsi-frontend/plugins");
    binary(root);
    text(root, "roms/Content.xyz", "Content, not a ROM");
    text(root, "_nds/dsi-frontend/plugins/xyz.ini",
         "[plugin]\nversion=1\nextensions=xyz\napplication=/apps/Probe.nds\nargv=0\n");
    text(root, "_nds/dsi-frontend/plugins/books.ini",
         "[plugin]\nversion=1\nextensions=fb2.zip,epub\napplication=/apps/Probe.nds\nargv=1\n");
    app_init(&app, root, true, true);
    check_profile_reads(root);
    char application[PATH_CAP], content[PATH_CAP];
    assert(path_join(application, root, "apps/Probe.nds"));
    assert(path_join(content, root, "roms/Content.xyz"));
    assert(association_find(&app, content) && !association_find(&app, content)->use_argv);
    const Association *book = association_find(&app, "sd:/media/books/Story.FB2.ZIP");
    assert(book && strstr(book->extensions, "fb2.zip"));
    assert(association_find(&app, "sd:/media/books/Story.epub") == book);
    assert(!association_find(&app, "sd:/media/books/Other.zip"));
    assert(!association_find(&app, "sd:/media/books/notfb2.zip"));
    /* A compound suffix wins even when a generic ZIP rule appears first. */
    app.associations[app.association_count++] = app.associations[0];
    path_copy(app.associations[0].extensions, "zip", sizeof app.associations[0].extensions);
    const Association *specific = association_find(&app, "sd:/media/books/Story.fb2.zip");
    assert(specific && strstr(specific->extensions, "fb2.zip"));
    assert(association_find(&app, "sd:/media/books/Other.zip") == &app.associations[0]);

    char book_profile[PATH_CAP];
    assert(path_join(book_profile, root, "_nds/dsi-frontend/plugins/books.ini"));
    assert(!remove(book_profile));
    associations_load(&app);
    assert(association_set_default(&app, "XYZ", application));
    assert(association_find(&app, content)->use_argv);
    assert(association_disable(&app, "xyz") && !association_find(&app, content));
    assert(association_user(&app, "xyz")->disabled);
    assert(association_clear_default(&app, "xyz"));
    assert(!association_user(&app, "xyz") && !association_find(&app, content)->use_argv);
    assert(!association_set_default(&app, "nds", application));
    assert(!association_set_default(&app, "../xyz", application));
    assert(!association_set_default(&app, "xyz", "/outside/app.nds"));
    app_category(&app, CATEGORY_GAMES);
    assert(!strcmp(app_selected(&app)->path, content));
    Location origin = app.locations[app.category];
    app_panel(&app, PANEL_ACTIONS);
    app_panel_move(&app, 7);
    app_panel_activate(&app);
    assert(app.association_editor.count == 1);
    association_editor_activate(&app);
    association_editor_activate(&app);
    assert(app.picker.open && app.picker.kind == PICKER_EXTENSION_APPLICATION);
    picker_activate(&app);
    assert(!app.picker.open && association_user(&app, "xyz"));
    assert(!memcmp(&origin, &app.locations[app.category], sizeof origin));
    render_app(&app, top, bottom, "12:00");
    app_back(&app); /* Extension options -> extension list. */
    assert(app.association_editor.open && app.association_editor.option == -1);
    app_back(&app); /* Extension list -> SELECT menu. */
    assert(!app.association_editor.open && app.panel == PANEL_ACTIONS && app.panel_selection == 7);
    app_back(&app);
    assert(app.panel == PANEL_NONE);
    LaunchPlan plan;
    char error[160];
    assert(launch_prepare(&app, content, &plan, error) && plan.arguments.count == 2);
    assert(!strcmp(plan.content, content));
    assert(!dashboard_record_request(&app, "/outside/file"));
    assert(dashboard_record_request(&app, content));
    assert(dashboard_record_request(&app, application));
    assert(dashboard_record_request(&app, content));
    assert(app.dashboard.recent_count == 2);
    assert(!strcmp(app.dashboard.recent[0].path, content));
    app_category(&app, CATEGORY_HOME);
    assert(app.entry_count == 7); /* Five browse links and two requests. */
    dashboard_open(&app);
    dashboard_move(&app, 3);
    dashboard_edit(&app, -1);
    assert(app.dashboard.config.order[2] == DASH_RECENT);
    dashboard_toggle(&app);
    assert(!app.dashboard.config.enabled[DASH_RECENT]);
    assert(dashboard_record_request(&app, application) && app.dashboard.recent_count == 2);
    dashboard_toggle(&app);
    dashboard_limit(&app, 1);
    assert(app.dashboard.config.limit[DASH_RECENT] == 1);
    assert(app.entry_count == 6 && app.dashboard.recent_count == 2);
    app_init(&restored, root, true, true);
    assert(restored.dashboard.config.limit[DASH_RECENT] == 1);
    dashboard_limit(&app, -1);
    assert(app.dashboard.config.limit[DASH_RECENT] == 0 && app.entry_count == 7);
    render_app(&app, top, bottom, "12:00");
    app_init(&restored, root, true, true);
    assert(restored.dashboard.config.order[2] == DASH_RECENT);
    assert(restored.dashboard.recent_count == 2);
    assert(association_user(&restored, "xyz"));
    assert(dashboard_clear_recent(&restored));
    assert(!restored.dashboard.recent_count);
    for (int i = 0; i < 4; i++) {
        restored.dashboard.selected = i;
        if (restored.dashboard.config.enabled[restored.dashboard.config.order[i]])
            dashboard_toggle(&restored);
    }
    app_category(&restored, CATEGORY_HOME);
    assert(restored.entry_count == 0);
    assert(!dashboard_record_request(&restored, "/outside/file") ||
           !restored.dashboard.recent_count);
    for (int i = 0; i < USER_ASSOCIATION_CAP - 1; i++) {
        char extension[16];
        snprintf(extension, sizeof extension, "e%02d", i);
        assert(association_set_default(&restored, extension, application));
    }
    assert(restored.user_association_count == USER_ASSOCIATION_CAP);
    assert(!association_set_default(&restored, "overflow", application));
    /* Stale pins remain visible and can be repaired without touching content. */
    char missing[PATH_CAP];
    assert(path_join(missing, root, "roms/Moved.xyz"));
    path_copy(app.favorites[app.favorite_count++], missing, PATH_CAP);
    assert(dashboard_record_request(&app, missing));
    app.dashboard.open = false;
    app_category(&app, CATEGORY_HOME);
    int stale = -1;
    for (int i = 0; i < app.entry_count; i++)
        if (app.entries[i].kind == ITEM_MISSING)
            stale = i;
    assert(stale >= 0);
    app.locations[CATEGORY_HOME].selected = stale;
    app_open(&app);
    assert(app.picker.open && app.picker.kind == PICKER_REPAIR);
    render_app(&app, top, bottom, "12:00");
    picker_close(&app);
    assert(!dashboard_relink(&app, missing, "/outside/file"));
    assert(!dashboard_relink(&app, missing, missing));
    assert(dashboard_relink(&app, missing, content));
    assert(!strcmp(app.favorites[0], content));
    assert(!strcmp(app.dashboard.recent[0].path, content));
    assert(dashboard_relink(&app, content, NULL));
    assert(app.favorite_count == 0);
    FILE *unchanged = fopen(content, "r");
    assert(unchanged && !fclose(unchanged));
    /* Presets change appearance only, reject malformed input before mutation,
     * and bypass recovery sessions to preserve normal appearance. */
    app.browser_style = BROWSER_GRID;
    app.icon_size = ICON_LARGE;
    app.reduced_motion = true;
    app.wallpaper_strength = 17;
    app.show_icon_labels = false;
    assert(appearance_save(&app, 1));
    app.settings.preset_slot = 1;
    char displayed[64];
    app.advanced_settings = true;
    for (int i = 0; i < settings_option_count(&app, 0); i++) {
        const SettingsOption *option = settings_option(&app, 0, i);
        if (!strcmp(option->label, "Preset slot")) {
            settings_value(&app, option, displayed, sizeof displayed);
            assert(!strcmp(displayed, "Evening"));
        }
    }

    Location location = app.locations[CATEGORY_HOME];
    appearance_reset(&app);
    assert(app.browser_style == BROWSER_CLASSIC && app.wallpaper_strength == -1 &&
           app.show_icon_labels);
    assert(appearance_load(&app, 1));
    assert(app.browser_style == BROWSER_GRID && app.icon_size == ICON_LARGE && app.reduced_motion);
    assert(app.wallpaper_strength == 17 && !app.show_icon_labels);
    assert(!strcmp(location.path, app.locations[CATEGORY_HOME].path));
    text(root, "_nds/dsi-frontend/appearance-3.ini",
         "version=1\nname=Bad\ntheme=../escape\nvalues=0,0,0,0,0,0,0,-1\ntop=\nbottom=\n");
    assert(!appearance_load(&app, 2) && app.browser_style == BROWSER_GRID);
    const char *bad_values[] = {
        "4294967296,0,0,0,0,0,0,-1",
        "0,4294967297,0,0,0,0,0,-1",
        "0,0,0,-4294967296,0,0,0,-1",
        "0,0,0,0,0,0,0,4294967295",
        "999999999999999999999999,0,0,0,0,0,0,-1",
        "0,0,0,0,999,0,0,-1",
        "0,0,0,0,0,0,0,-1trailing",
        "0,0,0,0,0,0,0",
    };
    for (unsigned i = 0; i < sizeof bad_values / sizeof *bad_values; i++) {
        char damaged[512];
        snprintf(damaged, sizeof damaged,
                 "version=1\nname=Bad numbers\ntheme=\nvalues=%s\ntop=\nbottom=\n", bad_values[i]);
        text(root, "_nds/dsi-frontend/appearance-3.ini", damaged);
        assert(!appearance_load(&app, 2));
        assert(app.browser_style == BROWSER_GRID && app.icon_size == ICON_LARGE &&
               app.reduced_motion && app.wallpaper_strength == 17 && !app.show_icon_labels);
    }
    /* An invalid primary still permits the existing recovery copy. */
    text(root, "_nds/dsi-frontend/appearance-3.bak",
         "version=1\nname=Recovery\ntheme=\nvalues=0,0,1,1,3,1,0,17\ntop=\nbottom=\nicon_labels="
         "0\n");
    assert(appearance_load(&app, 2));
    assert(app.browser_style == BROWSER_GRID && app.icon_size == ICON_LARGE && app.reduced_motion &&
           app.wallpaper_strength == 17 && !app.show_icon_labels);
    char preset_backup[PATH_CAP];
    assert(path_preferences_file(preset_backup, root, "appearance-3.bak"));
    assert(!remove(preset_backup));
    text(root, "_nds/dsi-frontend/appearance-3.ini",
         "version=1\nname=Legacy\ntheme=\nvalues=0,0,0,1,3,1,0,-1\ntop=\nbottom=\n");
    assert(appearance_load(&app, 2) && app.show_icon_labels);
    app.recovery_mode = true;
    assert(!appearance_load(&app, 1) && !appearance_save(&app, 1));
    appearance_reset(&app);
    assert(app.browser_style == BROWSER_GRID);
    app.recovery_mode = false;
    app_category(&app, CATEGORY_SYSTEM);
    app.locations[CATEGORY_SYSTEM].selected = 0;
    app_toggle_favorite(&app);
    assert(app_is_favorite(&app, "system:settings"));
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(app_is_favorite(&restored, "system:settings"));
    restored.dashboard.config.enabled[DASH_PINS] = true;
    app_category(&restored, CATEGORY_HOME);
    bool system_pin = false;
    for (int i = 0; i < restored.entry_count; i++)
        if (restored.entries[i].kind == ITEM_SYSTEM && restored.entries[i].target == 0) {
            system_pin = true;
            restored.locations[CATEGORY_HOME].selected = i;
        }
    assert(system_pin);
    app_open(&restored);
    assert(restored.system_discovery_requested && restored.system_title_requested == 0);
    assert(dashboard_relink(&restored, "system:settings", NULL));
    assert(!app_is_favorite(&restored, "system:settings"));
    assert(system_destination("system:menu") == 5 && system_destination("system:unknown") < 0);
    app.system_catalog.launcher_scan_failed = true;
    assert(diagnostics_export(&app));
    char report[PATH_CAP];
    assert(path_preferences_file(report, root, "diagnostics.txt"));
    FILE *report_file = fopen(report, "r");
    char report_text[4096] = {0};
    assert(report_file && fread(report_text, 1, sizeof report_text - 1, report_file) > 0);
    fclose(report_file);
    assert(strstr(report_text, "build=host-validation") && strstr(report_text, "sizeof_app="));
    assert(strstr(report_text, "nand_launcher_scan_failed=1\n"));
    assert(!strstr(report_text, root));
    app.storage_ready = false;
    assert(!diagnostics_export(&app));
    check_dashboard_recovery(root);
    check_recent_recovery(root);
    check_shortcut_merge(root);
    puts("PASS: extension editor/defaults/disable/restore, picker priority, dashboard layout and "
         "bounded request journal, stale shortcut repair/removal and isolated appearance presets");
}
