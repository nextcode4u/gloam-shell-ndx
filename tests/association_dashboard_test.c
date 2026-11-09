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
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

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
    assert(appearance_save(&app, 1));
    app.settings.preset_slot = 1;
    char displayed[64];
    for (int i = 0; i < settings_option_count(0); i++) {
        const SettingsOption *option = settings_option(0, i);
        if (!strcmp(option->label, "Preset slot")) {
            settings_value(&app, option, displayed, sizeof displayed);
            assert(!strcmp(displayed, "Evening"));
        }
    }

    Location location = app.locations[CATEGORY_HOME];
    appearance_reset(&app);
    assert(app.browser_style == BROWSER_CLASSIC && app.wallpaper_strength == -1);
    assert(appearance_load(&app, 1));
    assert(app.browser_style == BROWSER_GRID && app.icon_size == ICON_LARGE && app.reduced_motion);
    assert(app.wallpaper_strength == 17);
    assert(!strcmp(location.path, app.locations[CATEGORY_HOME].path));
    text(root, "_nds/dsi-frontend/appearance-3.ini",
         "version=1\nname=Bad\ntheme=../escape\nvalues=0,0,0,0,0,0,0,-1\ntop=\nbottom=\n");
    assert(!appearance_load(&app, 2) && app.browser_style == BROWSER_GRID);
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
    assert(diagnostics_export(&app));
    char report[PATH_CAP];
    assert(path_preferences_file(report, root, "diagnostics.txt"));
    FILE *report_file = fopen(report, "r");
    char report_text[4096] = {0};
    assert(report_file && fread(report_text, 1, sizeof report_text - 1, report_file) > 0);
    fclose(report_file);
    assert(strstr(report_text, "build=host-validation") && strstr(report_text, "sizeof_app="));
    assert(!strstr(report_text, root));
    app.storage_ready = false;
    assert(!diagnostics_export(&app));
    puts("PASS: extension editor/defaults/disable/restore, picker priority, dashboard layout and "
         "bounded request journal, stale shortcut repair/removal and isolated appearance presets");
}
