#include "app.h"
#include "artwork.h"
#include "associations.h"
#include "browser.h"
#include "item_preferences.h"
#include "launch.h"
#include "paths.h"
#include "picker.h"
#include "renderer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app;
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
    app_back(&app);
    assert(app.panel == PANEL_ACTIONS && app.panel_selection == 2);
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
    puts("PASS: independent picker, cover preview/assignment, per-file application, clearing and "
         "navigation");
}
