#include "app.h"
#include "artwork.h"
#include "associations.h"
#include "browser.h"
#include "item_preferences.h"
#include "launch.h"
#include "layout.h"
#include "paths.h"
#include "picker.h"
#include "renderer.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app;
static const char *watched_current, *watched_backup;
static FILE *failed_stream;
static unsigned record_opens;
static bool inject_failure;
static unsigned failure_mode;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_fstat(int, struct stat *);
size_t __real_fread(void *, size_t, size_t, FILE *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    bool watched = inject_failure && *mode == 'r' &&
                   ((watched_current && !strcmp(path, watched_current)) ||
                    (watched_backup && !strcmp(path, watched_backup))) &&
                   ++record_opens <= 2;
    if (watched && failure_mode == 0) {
        errno = EIO;
        return NULL;
    }
    FILE *file = __real_fopen(path, mode);
    if (watched)
        failed_stream = file;
    return file;
}
int __wrap_fclose(FILE *file)
{
    bool deny = file == failed_stream && failure_mode == 3;
    if (file == failed_stream)
        failed_stream = NULL;
    int result = __real_fclose(file);
    if (deny) {
        errno = EIO;
        return EOF;
    }
    return result;
}
int __wrap_fstat(int descriptor, struct stat *info)
{
    if (failed_stream && fileno(failed_stream) == descriptor && failure_mode == 1) {
        errno = EIO;
        return -1;
    }
    return __real_fstat(descriptor, info);
}
size_t __wrap_fread(void *data, size_t size, size_t count, FILE *file)
{
    return file == failed_stream && failure_mode == 2 ? 0 : __real_fread(data, size, count, file);
}
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
size_t __wrap___fread_chk(void *data, size_t capacity, size_t size, size_t count, FILE *file)
{
    return file == failed_stream && failure_mode == 2
               ? 0
               : __real___fread_chk(data, capacity, size, count, file);
}
#endif
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} top, bottom;
static void directory(const char *root, const char *name)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    assert(!mkdir(path, 0777));
}
static void file_text(const char *root, const char *name, const char *text)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    FILE *file = fopen(path, "w");
    assert(file && fputs(text, file) >= 0 && !fclose(file));
}
static int find_picker(const char *name)
{
    for (int i = 0; i < app.picker.count; i++)
        if (!strcmp(app.picker.entries[i].name, name))
            return i;
    assert(!"picker entry missing");
    return -1;
}
static void choose(const char *name)
{
    picker_move(&app, find_picker(name) - app.picker.selected);
    picker_activate(&app);
}
static void binary(const char *root)
{
    unsigned char bytes[0x1200] = {0};
    memcpy(bytes + 12, "####", 4);
    for (int cpu = 0; cpu < 2; cpu++) {
        uint32_t words[] = {0x1000 + cpu * 0x100, cpu ? 0x037F8000 : 0x02000000,
                            cpu ? 0x037F8000 : 0x02000000, 0x100};
        memcpy(bytes + 0x20 + cpu * 16, words, sizeof words);
    }
    char path[PATH_CAP];
    assert(path_join(path, root, "apps/Probe.nds"));
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes && !fclose(file));
}
static void check_menu_cursor(void)
{
    ThemePalette original = app.themes.active.palette;
    uint16_t selection_text = app.themes.active.selection_text;
    app.themes.active.selection_text = original.text;
    /* Glacier intentionally shares its separator and surface colors. A disabled
     * action must still show focus without changing whether it can be invoked. */
    app.themes.active.palette.line = app.themes.active.palette.surface;
    for (int panel = 0; panel < 2; panel++) {
        app_panel(&app, panel ? PANEL_QUICK : PANEL_ACTIONS);
        int count = panel ? QUICK_CONTROL_COUNT : ACTION_COUNT;
        for (int step = 0; step < count * 2; step++) {
            int selected = app.panel_selection;
            bool enabled = panel || app.action_enabled[selected];
            render_app(&app, top.pixels, bottom.pixels, "10:00");
            int row = layout_panel_top(&app) + 29 + (selected - app.panel_scroll) * 15;
            assert(bottom.pixels[row * 256 + 10] == app.themes.active.palette.accent);
            if (!enabled) {
                unsigned readable = 0;
                for (int y = row; y < row + FONT_HEIGHT; y++)
                    for (int x = 16; x < 230; x++)
                        readable += bottom.pixels[y * 256 + x] == original.text;
                assert(readable > 5);
            }
            assert(app.panel_selection == selected);
            assert(panel || app.action_enabled[selected] == enabled);
            app_panel_move(&app, step < count ? 1 : -1);
        }
        app_back(&app);
    }
    app.themes.active.palette = original;
    app.themes.active.selection_text = selection_text;
}
static void assignment_edit_failures(const char *root)
{
    const Entry *entry = app_selected(&app);
    assert(entry && entry->kind == ITEM_FILE);
    ItemPreferences original = {0}, actual;
    assert(path_join(original.cover, root, "roms/images/Good.png"));
    assert(path_join(original.icon, root, "Custom.png"));
    assert(path_join(original.application, root, "apps/Probe.nds"));
    char key[PATH_CAP], name[80], current[PATH_CAP], backup[PATH_CAP];
    const char *relative = entry->path + strlen(root);
    while (*relative == '/')
        relative++;
    assert(path_resolve_sd(key, "sd:/", relative));
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    snprintf(name, sizeof name, "items/%016" PRIx64 ".ini", hash);
    assert(path_preferences_file(current, root, name));
    strcpy(name + strlen(name) - 3, "bak");
    assert(path_preferences_file(backup, root, name));
    watched_current = current;
    watched_backup = backup;
    app_panel(&app, PANEL_NONE);
    for (unsigned action = 0; action < 6; action++) {
        for (failure_mode = 0; failure_mode < 4; failure_mode++) {
            assert(item_preferences_save(root, entry->path, &original));
            assert(item_preferences_save(root, entry->path, &original));
            if (action >= 3) {
                PickerKind kind = action == 3   ? PICKER_COVER
                                  : action == 4 ? PICKER_ICON
                                                : PICKER_APPLICATION;
                assert(picker_open(&app, kind));
                if (action == 3) {
                    choose("images");
                    picker_move(&app, find_picker("Good.png") - app.picker.selected);
                } else if (action == 4) {
                    picker_back(&app);
                    picker_move(&app, find_picker("Custom.png") - app.picker.selected);
                } else
                    picker_move(&app, find_picker("Probe.nds") - app.picker.selected);
            }
            record_opens = 0;
            inject_failure = true;
            if (action == 0)
                assert(!picker_clear_assignment(&app, true));
            else if (action == 1)
                assert(!picker_clear_assignment(&app, false));
            else if (action == 2)
                assert(!picker_clear_icon(&app));
            else {
                picker_activate(&app);
                assert(app.picker.open && strstr(app.picker.status, "read"));
            }
            inject_failure = false;
            assert(record_opens == 2);
            assert(item_preferences_load(root, entry->path, &actual));
            assert(!strcmp(actual.cover, original.cover) && !strcmp(actual.icon, original.icon) &&
                   !strcmp(actual.application, original.application));
            if (action == 0)
                assert(picker_clear_assignment(&app, true));
            else if (action == 1)
                assert(picker_clear_assignment(&app, false));
            else if (action == 2)
                assert(picker_clear_icon(&app));
            else {
                picker_activate(&app);
                assert(!app.picker.open);
                app_back(&app);
            }
            assert(item_preferences_load(root, entry->path, &actual));
            assert(!strcmp(actual.application, action == 0 ? "" : original.application));
            assert(!strcmp(actual.cover, action == 1 ? "" : original.cover));
            assert(!strcmp(actual.icon, action == 2 ? "" : original.icon));
        }
    }
    watched_current = watched_backup = NULL;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    directory(root, "apps");
    /* roms/images already contains the PNG supplied by the host harness. */

    directory(root, "roms/other");
    file_text(root, "roms/A.txt", "Content");
    file_text(root, "roms/other/A.txt", "Same basename, different content identity");
    file_text(root, "roms/images/Broken.png", "Not an image");
    binary(root);
    app_init(&app, root, true, true);
    app_category(&app, CATEGORY_GAMES);
    int selected = 0;
    while (strcmp(app.entries[selected].name, "A.txt"))
        selected++;
    app_move(&app, selected);
    app_page(&app, -1);
    assert(app.locations[app.category].selected == 0);
    app_page(&app, 1);
    assert(app.locations[app.category].selected == app.entry_count - 1);
    app_move(&app, selected - app.locations[app.category].selected);
    Location origin = app.locations[app.category];
    Entry content = *app_selected(&app);
    ItemPreferences item;
    assert(!item_preferences_load(root, content.path, &item));
    check_menu_cursor();
    app_panel(&app, PANEL_ACTIONS);
    app.panel_selection = 3;
    app_panel_activate(&app);
    assert(app.picker.open && app.picker.kind == PICKER_COVER);
    assert(app.picker.count == 2); /* Only directories, not .txt content. */
    choose("images");
    assert(app.picker.count == 2); /* The host script supplies Good.png. */
    choose("Broken.png");
    assert(app.picker.open && *app.picker.status);
    assert(!item_preferences_load(root, content.path, &item));
    picker_move(&app, find_picker("Good.png") - app.picker.selected);
    for (int i = 0; i < 7; i++)
        picker_tick(&app);
    assert(app.picker.preview.ready);
    top.before = top.after = bottom.before = bottom.after = 0xCAFEABCD;
    render_app(&app, top.pixels, bottom.pixels, "10:00");
    assert(top.before == 0xCAFEABCD && top.after == 0xCAFEABCD);
    assert(bottom.before == 0xCAFEABCD && bottom.after == 0xCAFEABCD);
    uint16_t footer_ink = app.themes.active.footer_text;
    app.themes.active.footer_text = 0x801f;
    render_app(&app, top.pixels, bottom.pixels, "10:00");
    unsigned footer_pixels = 0, top_footer_pixels = 0;
    for (int y = 176; y < 192; y++)
        for (int x = 10; x < 241; x++) {
            footer_pixels += bottom.pixels[y * 256 + x] == 0x801f;
            top_footer_pixels += top.pixels[y * 256 + x] == 0x801f;
        }
    assert(footer_pixels > 5 && top_footer_pixels > 5);
    app.themes.active.footer_text = footer_ink;
    picker_touch(&app, 10, 189); /* Footer cannot select a file. */
    assert(app.picker.open);
    picker_activate(&app);
    assert(!app.picker.open && app.panel == PANEL_MESSAGE);
    app_back(&app); /* Assignment confirmation -> SELECT menu, not browser. */
    assert(app.panel == PANEL_ACTIONS && app.panel_selection == 3);
    assert(app.action_enabled[4]); /* Clear cover is refreshed after assigning one. */
    app.panel_selection = 2;
    app_panel_activate(&app);
    assert(app.panel == PANEL_INFO);
    assert(!app.inspection); /* Opening a path must not scan launch binaries. */
    app_panel_adjust(&app, 1);
    assert(app.panel_selection == 1 && app.inspection);
    render_app(&app, top.pixels, bottom.pixels, "10:00");
    assert(top.before == 0xCAFEABCD && top.after == 0xCAFEABCD);
    assert(bottom.before == 0xCAFEABCD && bottom.after == 0xCAFEABCD);
    /* Long diagnostic paths must keep moving after the panel's entry animation. */
    for (size_t i = 0; i + 1 < sizeof app.inspection->application; i++)
        app.inspection->application[i] = 'a' + i % 26;
    app.inspection->application[sizeof app.inspection->application - 1] = 0;
    bool reduced_motion = app.reduced_motion;
    app.reduced_motion = false;
    static uint16_t path_before[256 * 12], path_after[256 * 12], status_before[256 * 12];
    path_copy(app.inspection->status,
              "Launching requires DSi mode with SD access. Start Gloam through a DSi SD boot "
              "method, then retry.",
              sizeof app.inspection->status);
    for (unsigned i = 0; i < 30; i++)
        app_tick(&app);
    render_app(&app, top.pixels, bottom.pixels, "10:00");
    memcpy(path_before, top.pixels + 150 * 256, sizeof path_before);
    memcpy(status_before, bottom.pixels + 152 * 256, sizeof status_before);
    for (unsigned i = 0; i < 180; i++)
        app_tick(&app);
    render_app(&app, top.pixels, bottom.pixels, "10:00");
    memcpy(path_after, top.pixels + 150 * 256, sizeof path_after);
    assert(memcmp(path_before, path_after, sizeof path_before));
    assert(memcmp(status_before, bottom.pixels + 152 * 256, sizeof status_before));
    app.reduced_motion = true;
    render_app(&app, top.pixels, bottom.pixels, "10:00");
    memcpy(path_before, top.pixels + 150 * 256, sizeof path_before);
    memcpy(status_before, bottom.pixels + 152 * 256, sizeof status_before);
    for (unsigned i = 0; i < 180; i++)
        app_tick(&app);
    render_app(&app, top.pixels, bottom.pixels, "10:00");
    assert(!memcmp(path_before, top.pixels + 150 * 256, sizeof path_before));
    assert(!memcmp(status_before, bottom.pixels + 152 * 256, sizeof status_before));
    app.reduced_motion = reduced_motion;
    app_panel_adjust(&app, 1);
    assert(app.panel_selection == 0);
    app_panel_adjust(&app, 1);
    assert(app.panel_selection == 1 && app.panel_text_started_frame == app.frame_count);
    app_back(&app);
    assert(app.panel == PANEL_ACTIONS && app.panel_selection == 2 && !app.inspection);
    app.panel_selection = 4;
    app_panel_activate(&app);
    assert(app.panel == PANEL_MESSAGE);
    app_panel_activate(&app); /* A also dismisses confirmations by one level. */
    assert(app.panel == PANEL_ACTIONS && app.panel_selection == 4 && !app.action_enabled[4]);
    app_back(&app);
    assert(app.panel == PANEL_NONE);
    /* Ordinary browser messages must not reopen a previous SELECT menu. */
    app_panel(&app, PANEL_MESSAGE);
    app_back(&app);
    assert(app.panel == PANEL_NONE);
    app_panel(&app, PANEL_ACTIONS);
    app_panel_move(&app, 3);
    app_panel_activate(&app);
    choose("images");
    choose("Good.png");
    picker_activate(&app);
    app_back(&app);
    assert(app.panel == PANEL_ACTIONS && app.panel_selection == 3);
    assert(!memcmp(&origin, &app.locations[app.category], sizeof origin));
    assert(!strcmp(content.path, app_selected(&app)->path));
    assert(item_preferences_load(root, content.path, &item) && strstr(item.cover, "Good.png"));
    artwork_load(&app);
    assert(app.artwork.ready && !strcmp(app.artwork.source, "Assigned image"));
    char same_name[PATH_CAP];
    assert(path_join(same_name, root, "roms/other/A.txt"));
    ItemPreferences other;
    assert(!item_preferences_load(root, same_name, &other));
    assert(picker_open(&app, PICKER_APPLICATION));
    choose("Probe.nds");
    assert(!app.picker.open);
    assert(item_preferences_load(root, content.path, &item) && *item.application && *item.cover);
    LaunchPlan plan;
    char error[160], icon[PATH_CAP];
    assert(launch_prepare(&app, content.path, &plan, error));
    assert(plan.arguments.count == 2);
    assert(!strcmp(plan.application, item.application));
    assert(!strcmp(plan.arguments.data + plan.arguments.offsets[1], content.path));
    assert(association_icon_application(&app, content.path, icon));
    assert(!strcmp(icon, item.application));
    assert(picker_clear_assignment(&app, false));
    assert(item_preferences_load(root, content.path, &item) && !*item.cover && *item.application);
    assert(picker_clear_assignment(&app, true));
    assert(!launch_prepare(&app, content.path, &plan, error));
    assert(picker_open(&app, PICKER_COVER));
    picker_back(&app); /* roms -> SD root */
    assert(app.picker.open);
    picker_back(&app); /* root -> originating view */
    assert(!app.picker.open && !memcmp(&origin, &app.locations[app.category], sizeof origin));
    assert(picker_open(&app, PICKER_COVER));
    picker_close(&app);
    assert(!app.picker.entries && !strcmp(content.path, app_selected(&app)->path));
    path_copy(item.cover, "/outside/root.png", sizeof item.cover);
    assert(!item_preferences_save(root, content.path, &item));
    assignment_edit_failures(root);
    puts("PASS: independent picker, cover preview/assignment, per-file application, clearing and "
         "navigation");
}
