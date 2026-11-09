#include "app.h"
#include "browser.h"
#include "directory_scan.h"
#include "paths.h"
#include "picker.h"
#include "preferences.h"
#include "renderer.h"
#include "settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>

static App app, restored;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} top, bottom;
static void directory(const char *root, const char *name)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    assert(mkdir(path, 0777) == 0);
}
static void text_file(const char *root, const char *name)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    FILE *file = fopen(path, "wb");
    assert(file);
    fputs("Settings fixture", file);
    assert(fclose(file) == 0);
}
static int find(const char *name)
{
    for (int i = 0; i < app.entry_count; i++)
        if (!strcmp(app.entries[i].name, name))
            return i;
    return -1;
}
static void option(int section, int index)
{
    app.settings.section = section;
    app.settings.selected = index;
    app.settings.scroll = 0;
}
static bool select_setting(App *target, const char *label)
{
    for (int page = 0; page < settings_section_count(target); page++)
        for (int row = 0; row < settings_option_count(target, page); row++)
            if (!strcmp(settings_option(target, page, row)->label, label)) {
                target->settings.section = page;
                target->settings.selected = row;
                target->settings.scroll = row > 6 ? row - 6 : 0;
                return true;
            }
    return false;
}
static void check_settings_levels(const char *root)
{
    static App levels, loaded;
    char isolated[PATH_CAP], state_path[PATH_CAP];
    assert(path_join(isolated, root, "settings-levels") && !mkdir(isolated, 0777));
    app_init(&levels, isolated, true, true);
    assert(settings_is_simple(&levels) && !levels.advanced_settings &&
           !levels.simple_settings_lock);
    settings_open(&levels);
    assert(settings_section_count(&levels) == 4);
    const char *technical[] = {"Game defaults",        "Emulator profiles",   "Unlaunch setup",
                               "Import configuration", "Reset configuration", "Folder tabs",
                               "Auto-launch recent",   "Show hidden",         "Show system files"};
    for (unsigned i = 0; i < sizeof technical / sizeof *technical; i++)
        assert(!select_setting(&levels, technical[i]));
    assert(select_setting(&levels, "Accent"));
    int accent = levels.accent_index;
    settings_adjust(&levels, 1);
    assert(levels.accent_index != accent);
    levels.auto_launch_delay = 5; /* A configured technical choice must survive view changes. */
    settings_open(&levels);
    settings_section(&levels, -1);
    assert(levels.settings.section == 3);
    settings_touch(&levels, 190, 27);
    assert(levels.settings.section == 0);
    assert(select_setting(&levels, "Settings mode"));
    settings_activate(&levels);
    assert(!settings_is_simple(&levels) && settings_section_count(&levels) == 7);
    assert(levels.auto_launch_delay == 5 && select_setting(&levels, "Game defaults"));
    assert(select_setting(&levels, "Settings mode"));
    settings_adjust(&levels, -1);
    assert(settings_is_simple(&levels) && levels.settings.section == 0);
    assert(app_save(&levels));
    app_init(&loaded, isolated, true, true);
    assert(settings_is_simple(&loaded) && !loaded.simple_settings_lock);
    assert(loaded.auto_launch_delay == 5 && loaded.accent_index == levels.accent_index);
    assert(path_preferences_file(state_path, isolated, "state.ini"));
    FILE *state = fopen(state_path, "w");
    assert(state &&
           fputs("version=1\nsettings_mode=advanced\nsimple_settings_lock=1\n"
                 "auto_launch_delay=5\n",
                 state) >= 0 &&
           !fclose(state));
    app_init(&levels, isolated, true, true);
    assert(levels.advanced_settings && levels.simple_settings_lock && settings_is_simple(&levels));
    settings_open(&levels);
    assert(!select_setting(&levels, "Settings mode"));
    for (unsigned i = 0; i < sizeof technical / sizeof *technical; i++)
        assert(!select_setting(&levels, technical[i]));
    levels.settings.section = 6; /* A stale Advanced page cannot expose its action. */
    levels.settings.selected = 0;
    assert(!settings_option(&levels, 6, 0));
    settings_adjust(&levels, 1);
    settings_activate(&levels);
    assert(!levels.configuration_reload_requested && !levels.system_discovery_requested);
    settings_open(&levels);
    for (int page = 0; page < 12; page++) {
        settings_section(&levels, 1);
        assert(levels.settings.section < 4);
        for (int row = 0; row < settings_option_count(&levels, levels.settings.section); row++) {
            levels.settings.selected = row;
            top.before = bottom.before = 0x12345678;
            top.after = bottom.after = 0x87654321;
            settings_render(&levels, top.pixels, bottom.pixels);
            assert(top.before == 0x12345678 && bottom.before == 0x12345678);
            assert(top.after == 0x87654321 && bottom.after == 0x87654321);
        }
    }
    assert(app_save(&levels));
    app_init_mode(&loaded, isolated, true, true, true);
    assert(loaded.simple_settings_lock && settings_is_simple(&loaded));
    assert(app_save(&loaded)); /* Recovery saves retain the administrative lock too. */
    app_init(&loaded, isolated, true, true);
    assert(loaded.simple_settings_lock && loaded.advanced_settings && settings_is_simple(&loaded));
    state = fopen(state_path, "w");
    assert(state && fputs("version=1\n", state) >= 0 && !fclose(state));
    app_init(&loaded, isolated, true, true);
    assert(loaded.advanced_settings && !loaded.simple_settings_lock &&
           !settings_is_simple(&loaded));
    state = fopen(state_path, "w");
    assert(state &&
           fputs("version=1\nsettings_mode=simple\nsimple_settings_lock=0\n", state) >= 0 &&
           !fclose(state));
    app_init(&loaded, isolated, true, true);
    assert(settings_is_simple(&loaded) && !loaded.simple_settings_lock);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    directory(root, "roms");
    directory(root, "roms/Folder");
    text_file(root, "roms/Alpha.nds");
    text_file(root, "roms/Zeta.txt");
    text_file(root, "roms/.hidden.txt");
    text_file(root, "roms/Unrecognized.xyz");
    char path[PATH_CAP];
    assert(path_join(path, root, "roms/Alpha.nds"));
    struct utimbuf time = {1, 1};
    assert(utime(path, &time) == 0);
    app_init(&app, root, true, true);
    app_category(&app, CATEGORY_GAMES);
    assert(app.show_covers && app.show_extensions && !app.show_hidden);
    assert(find(".hidden.txt") == -1);
    app_move(&app, find("Alpha.nds") - app.locations[1].selected);
    app.advanced_settings = true;
    settings_open(&app);
    assert(app.settings.open && app.settings.section == 0);
    settings_section(&app, 1);
    assert(app.settings.section == 1);
    option(1, 0);
    settings_adjust(&app, 1);
    assert(app.sort_mode == SORT_NAME_DESC);
    assert(!strcmp(app_selected(&app)->name, "Alpha.nds"));
    assert(!strcmp(app.entries[0].name, "Folder"));
    assert(!strcmp(app.entries[1].name, "Zeta.txt"));
    settings_adjust(&app, 1);
    assert(app.sort_mode == SORT_TYPE);
    settings_adjust(&app, 1);
    assert(app.sort_mode == SORT_MODIFIED);
    assert(!strcmp(app.entries[app.entry_count - 1].name, "Alpha.nds"));
    option(1, 1);
    settings_adjust(&app, 1);
    assert(app.show_hidden && find(".hidden.txt") >= 0);
    option(1, 2);
    settings_adjust(&app, 1);
    assert(app.show_system);
    option(1, 3);
    settings_adjust(&app, 1);
    assert(!app.show_extensions);
    char display[256];
    browser_display_name(&app, app_selected(&app), display);
    assert(!strcmp(display, "Alpha") && !strcmp(app_selected(&app)->name, "Alpha.nds"));
    option(1, 4);
    settings_adjust(&app, 1);
    assert(app.filter_recognized && find("Unrecognized.xyz") == -1);
    assert(find("Alpha.nds") >= 0 && find("Folder") >= 0);
    option(0, 4);
    settings_adjust(&app, 1);
    assert(!app.show_covers);
    option(0, 5);
    settings_adjust(&app, 1);
    assert(app.clock_12_hour);
    char clock[12];
    settings_format_clock(clock, 0, 3, true);
    assert(!strcmp(clock, "12:03 AM"));
    settings_format_clock(clock, 12, 3, true);
    assert(!strcmp(clock, "12:03 PM"));
    settings_format_clock(clock, 23, 59, true);
    assert(!strcmp(clock, "11:59 PM"));
    settings_format_clock(clock, 0, 3, false);
    assert(!strcmp(clock, "00:03"));
    option(1, 8);
    settings_adjust(&app, 1);
    assert(!strcmp(app.startup_tab, "home"));
    int origin = app.category;
    settings_back(&app);
    assert(!app.settings.open && app.category == origin && !app.preferences_dirty);
    app_init(&restored, root, true, true);
    assert(restored.category == CATEGORY_HOME && restored.clock_12_hour && !restored.show_covers);
    assert(restored.show_hidden && restored.show_system && !restored.show_extensions);
    assert(restored.filter_recognized && restored.sort_mode == SORT_MODIFIED);
    app.category = CATEGORY_GAMES;
    app_scan(&app);
    app.advanced_settings = true;
    settings_open(&app);
    /* Placeholder sections explain availability; they cannot silently change settings. */
    option(2, 5);
    app.preferences_dirty = false;
    settings_adjust(&app, 1);
    settings_activate(&app);
    assert(!app.preferences_dirty);
    for (int section = 0; section < SETTINGS_SECTION_COUNT; section++) {
        option(section, 0);
        for (int index = 0; index < settings_option_count(&app, section); index++) {
            app.settings.selected = index;
            top.before = top.after = bottom.before = bottom.after = 0x12345678;
            render_app(&app, top.pixels, bottom.pixels, "12:03 AM");
            assert(top.before == 0x12345678 && top.after == 0x12345678);
            assert(bottom.before == 0x12345678 && bottom.after == 0x12345678);
        }
    }
    option(0, 0);
    settings_touch(&app, 190, 27);
    assert(app.settings.section == 1);
    settings_touch(&app, 20, SETTINGS_LIST_TOP + SETTINGS_ROW_HEIGHT);
    assert(app.settings.selected == 1);
    bool hidden = app.show_hidden;
    settings_touch(&app, 20, SETTINGS_LIST_TOP + SETTINGS_ROW_HEIGHT);
    assert(app.show_hidden != hidden);
    option(1, 0);
    int selected = app.settings.selected;
    settings_touch(&app, 20, 180);
    assert(app.settings.selected == selected);
    settings_section(&app, -1);
    assert(app.settings.section == 0);
    settings_section(&app, -1);
    assert(app.settings.section == SETTINGS_SECTION_COUNT - 1);
    option(4, 0);
    settings_activate(&app);
    assert(!app.settings.open && app.category == CATEGORY_SYSTEM);
    /* The route page reports observed installation/discovery, never success. */
    app.advanced_settings = true;
    settings_open(&app);
    settings_section(&app, -1);
    assert(!strcmp(settings_section_name(&app, app.settings.section), "Launch status"));
    assert(app.system_discovery_requested);
    char route_value[96];
    settings_value(&app, settings_option(&app, app.settings.section, 1), route_value,
                   sizeof route_value);
    assert(!strcmp(route_value, "Missing/invalid"));
    app.system_catalog.attempted = true;
    app.system_catalog.titles[0].available = true;
    settings_value(&app, settings_option(&app, app.settings.section, 4), route_value,
                   sizeof route_value);
    assert(!strcmp(route_value, "Title found"));
    app.storage_lost = true;
    settings_value(&app, settings_option(&app, app.settings.section, 1), route_value,
                   sizeof route_value);
    assert(!strcmp(route_value, "SD unavailable"));
    app.storage_lost = false;
    /* Save folders are presentation-only, exact-name and case-insensitive.
     * Files with similar names and ordinary library folders remain visible. */
    directory(root, "roms/saves");
    directory(root, "roms/SAVESTATES");
    directory(root, "roms/Saves collection");
    text_file(root, "roms/saves.nds");
    assert(path_join(path, root, "roms/saves/Keep.sav"));
    FILE *save = fopen(path, "w");
    assert(save && fputs("Earned progress", save) >= 0 && !fclose(save));
    app_init(&app, root, true, true);
    app_category(&app, CATEGORY_GAMES);
    assert(app.hide_save_folders && find("saves") < 0 && find("SAVESTATES") < 0);
    assert(find("Saves collection") >= 0 && find("saves.nds") >= 0);
    DirectoryRecord record = {.directory = true};
    strcpy(record.name, "SaVeS");
    assert(directory_scan_is_save_folder(&record));
    record.directory = false;
    assert(!directory_scan_is_save_folder(&record));
    app_move(&app, find("Alpha.nds") - app.locations[CATEGORY_GAMES].selected);
    assert(picker_open(&app, PICKER_COVER));
    while (app.picker.directory)
        picker_scan_step(&app);
    for (int i = 0; i < app.picker.count; i++)
        assert(strcmp(app.picker.entries[i].name, "saves") &&
               strcmp(app.picker.entries[i].name, "SAVESTATES"));
    picker_close(&app);
    app.advanced_settings = true;
    settings_open(&app);
    option(1, 5);
    assert(!strcmp(settings_option(&app, 1, 5)->label, "Hide save folders"));
    settings_adjust(&app, 1);
    assert(!app.hide_save_folders && find("saves") >= 0 && find("SAVESTATES") >= 0);
    assert(!strcmp(app_selected(&app)->name, "Alpha.nds"));
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(!restored.hide_save_folders);
    assert(picker_open(&app, PICKER_COVER));
    while (app.picker.directory)
        picker_scan_step(&app);
    bool found_save_folder = false;
    for (int i = 0; i < app.picker.count; i++)
        found_save_folder |= !strcmp(app.picker.entries[i].name, "saves");
    assert(found_save_folder);
    picker_close(&app);
    settings_adjust(&app, -1);
    assert(app.hide_save_folders && find("saves") < 0 && find("SAVESTATES") < 0);
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(restored.hide_save_folders);
    /* BIOS visibility is an independent, case-insensitive directory filter. */
    assert(path_join(path, root, "roms/BiOs"));
    assert(!mkdir(path, 0777));
    assert(path_join(path, root, "roms/BIOS collection"));
    assert(!mkdir(path, 0777));
    text_file(root, "roms/bios.nds");
    assert(path_join(path, root, "roms/BiOs/firmware.bin"));
    FILE *bios = fopen(path, "w");
    assert(bios && fputs("Synthetic BIOS fixture", bios) >= 0 && !fclose(bios));
    app_init(&app, root, true, true);
    app_category(&app, CATEGORY_GAMES);
    assert(app.hide_bios_folders && find("BiOs") < 0);
    assert(find("BIOS collection") >= 0 && find("bios.nds") >= 0);
    record.directory = true;
    strcpy(record.name, "BIOS");
    assert(directory_scan_is_bios_folder(&record));
    record.directory = false;
    assert(!directory_scan_is_bios_folder(&record));
    assert(picker_open(&app, PICKER_COVER));
    while (app.picker.directory)
        picker_scan_step(&app);
    for (int i = 0; i < app.picker.count; i++)
        assert(strcmp(app.picker.entries[i].name, "BiOs"));
    picker_close(&app);
    app.advanced_settings = true;
    settings_open(&app);
    option(1, 6);
    assert(!strcmp(settings_option(&app, 1, 6)->label, "Hide BIOS folders"));
    settings_adjust(&app, 1);
    assert(!app.hide_bios_folders && app.hide_save_folders && find("BiOs") >= 0);
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(!restored.hide_bios_folders && restored.hide_save_folders);
    assert(picker_open(&app, PICKER_COVER));
    while (app.picker.directory)
        picker_scan_step(&app);
    bool found_bios = false;
    for (int i = 0; i < app.picker.count; i++)
        found_bios |= !strcmp(app.picker.entries[i].name, "BiOs");
    assert(found_bios);
    picker_close(&app);
    settings_adjust(&app, -1);
    assert(app.hide_bios_folders && find("BiOs") < 0);
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(restored.hide_bios_folders);
    bios = fopen(path, "r");
    char bios_contents[32] = "";
    assert(bios &&
           fread(bios_contents, 1, sizeof bios_contents - 1, bios) ==
               strlen("Synthetic BIOS fixture") &&
           !fclose(bios));
    assert(!strcmp(bios_contents, "Synthetic BIOS fixture"));
    assert(path_join(path, root, "roms/saves/Keep.sav"));
    char contents[32] = "";
    save = fopen(path, "r");
    assert(save && fread(contents, 1, sizeof contents - 1, save) == strlen("Earned progress") &&
           !fclose(save));
    assert(!strcmp(contents, "Earned progress"));
    check_settings_levels(root);
    puts("PASS: Simple/Advanced settings, config-only lock, migration, recovery persistence; full "
         "settings, sorting/selection, filters, display names, clock, startup and "
         "persistence");
    return 0;
}
