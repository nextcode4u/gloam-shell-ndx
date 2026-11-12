/* Full settings controller. Every editable option changes real frontend behavior. */
#include "settings.h"
#include "app.h"
#include "appearance.h"
#include "artwork.h"
#include "artwork_disk_cache.h"
#include "association_editor.h"
#include "associations.h"
#include "audio.h"
#include "browser.h"
#include "cheats.h"
#include "configuration.h"
#include "configuration_reset.h"
#include "dashboard.h"
#include "diagnostics.h"
#include "game_options.h"
#include "icons.h"
#include "launch.h"
#include "paths.h"
#include "picker.h"
#include "preferences.h"
#include "tabs.h"
#include "themes.h"
#include "ui_strings.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifndef HOST_PREVIEW
#include <blocksds_version.h>
#endif

enum {
    OPT_SETTINGS_MODE,
    OPT_AUTO_LAUNCH,
    OPT_CONFIG_EXPORT,
    OPT_CONFIG_IMPORT,
    OPT_RESET_CONFIGURATION,
    OPT_DIAGNOSTICS,
    OPT_RESET_BROWSER,
    OPT_CLEAR_RECENT,
    OPT_CLEAR_CACHES,
    OPT_PRESET_SLOT,
    OPT_PRESET_SAVE,
    OPT_PRESET_LOAD,
    OPT_RESET_APPEARANCE,
    OPT_ACCENT,
    OPT_MOTION,
    OPT_ICONS,
    OPT_VIEW,
    OPT_COVERS,
    OPT_CLOCK,
    OPT_SORT,
    OPT_HIDDEN,
    OPT_SYSTEM_FILES,
    OPT_EXTENSIONS,
    OPT_FILTER,
    OPT_HIDE_SAVES,
    OPT_HIDE_BIOS,
    OPT_SPEED,
    OPT_STARTUP,
    OPT_AUDIO,
    OPT_PROFILES,
    OPT_LAUNCH,
    OPT_LOADERS,
    OPT_CHEATS,
    OPT_SYSTEM,
    OPT_SYSTEM_MENU,
    OPT_ENVIRONMENT,
    OPT_UNLAUNCH,
    OPT_REFRESH,
    OPT_SAVE,
    OPT_ABOUT,
    OPT_TABS,
    OPT_TOOLCHAIN,
    OPT_THEME,
    OPT_DASHBOARD,
    OPT_MUSIC_MODE,
    OPT_MUSIC_PATH,
    OPT_MUSIC_VOLUME,
    OPT_EFFECTS,
    OPT_EFFECTS_VOLUME,
    OPT_BRIGHTNESS,
    OPT_BATTERY,
    OPT_SLEEP,
    OPT_TITLE_SOURCE,
    OPT_TITLE_LANGUAGE,
    OPT_TOP_BACKGROUND,
    OPT_BOTTOM_BACKGROUND,
    OPT_WALL_STRENGTH,
    OPT_RESET_WALLPAPERS,
    OPT_ROUTE_DIRECT,
    OPT_ROUTE_RETAIL,
    OPT_ROUTE_HOMEBREW,
    OPT_ROUTE_SLOT1,
    OPT_ROUTE_SETTINGS,
    OPT_ROUTE_CAMERA,
    OPT_ROUTE_SOUND,
    OPT_ROUTE_PICTO,
    OPT_ROUTE_DOWNLOAD,
    OPT_ROUTE_MENU
};

static const char *option_keys[] = {
    [OPT_SETTINGS_MODE] = "settings_mode",
    [OPT_AUTO_LAUNCH] = "auto_launch",
    [OPT_CONFIG_EXPORT] = "config_export",
    [OPT_CONFIG_IMPORT] = "config_import",
    [OPT_RESET_CONFIGURATION] = "reset_configuration",
    [OPT_DIAGNOSTICS] = "diagnostics",
    [OPT_RESET_BROWSER] = "reset_browser",
    [OPT_CLEAR_RECENT] = "clear_recent",
    [OPT_CLEAR_CACHES] = "clear_caches",
    [OPT_PRESET_SLOT] = "preset_slot",
    [OPT_PRESET_SAVE] = "preset_save",
    [OPT_PRESET_LOAD] = "preset_load",
    [OPT_RESET_APPEARANCE] = "reset_appearance",
    [OPT_ACCENT] = "accent",
    [OPT_MOTION] = "motion",
    [OPT_ICONS] = "icons",
    [OPT_VIEW] = "view",
    [OPT_COVERS] = "covers",
    [OPT_CLOCK] = "clock",
    [OPT_SORT] = "sort",
    [OPT_HIDDEN] = "hidden",
    [OPT_SYSTEM_FILES] = "system_files",
    [OPT_EXTENSIONS] = "extensions",
    [OPT_FILTER] = "filter",
    [OPT_HIDE_SAVES] = "hide_saves",
    [OPT_HIDE_BIOS] = "hide_bios",
    [OPT_SPEED] = "speed",
    [OPT_STARTUP] = "startup",
    [OPT_AUDIO] = "audio",
    [OPT_PROFILES] = "profiles",
    [OPT_LAUNCH] = "launch",
    [OPT_LOADERS] = "loaders",
    [OPT_CHEATS] = "cheats",
    [OPT_SYSTEM] = "system",
    [OPT_SYSTEM_MENU] = "system_menu",
    [OPT_ENVIRONMENT] = "environment",
    [OPT_UNLAUNCH] = "unlaunch",
    [OPT_REFRESH] = "refresh",
    [OPT_SAVE] = "save",
    [OPT_ABOUT] = "about",
    [OPT_TABS] = "tabs",
    [OPT_TOOLCHAIN] = "toolchain",
    [OPT_THEME] = "theme",
    [OPT_DASHBOARD] = "dashboard",
    [OPT_MUSIC_MODE] = "music_mode",
    [OPT_MUSIC_PATH] = "music_path",
    [OPT_MUSIC_VOLUME] = "music_volume",
    [OPT_EFFECTS] = "effects",
    [OPT_EFFECTS_VOLUME] = "effects_volume",
    [OPT_BRIGHTNESS] = "brightness",
    [OPT_BATTERY] = "battery",
    [OPT_SLEEP] = "sleep",
    [OPT_TITLE_SOURCE] = "title_source",
    [OPT_TITLE_LANGUAGE] = "title_language",
    [OPT_TOP_BACKGROUND] = "top_background",
    [OPT_BOTTOM_BACKGROUND] = "bottom_background",
    [OPT_WALL_STRENGTH] = "wall_strength",
    [OPT_RESET_WALLPAPERS] = "reset_wallpapers",
    [OPT_ROUTE_DIRECT] = "route_direct",
    [OPT_ROUTE_RETAIL] = "route_retail",
    [OPT_ROUTE_HOMEBREW] = "route_homebrew",
    [OPT_ROUTE_SLOT1] = "route_slot1",
    [OPT_ROUTE_SETTINGS] = "route_settings",
    [OPT_ROUTE_CAMERA] = "route_camera",
    [OPT_ROUTE_SOUND] = "route_sound",
    [OPT_ROUTE_PICTO] = "route_picto",
    [OPT_ROUTE_DOWNLOAD] = "route_download",
    [OPT_ROUTE_MENU] = "route_menu",
};
static const char *section_names[] = {"Appearance",   "Browser & Home", "Audio",
                                      "Games & Apps", "Boot & System",  "Maintenance",
                                      "Launch status"};
static const SettingsOption appearance[] = {
    {"Accent", "Choose a personal accent override. The installed theme package is left unchanged.",
     true, OPT_ACCENT},
    {"Reduced motion", "Use immediate selection changes and simpler panel movement.", true,
     OPT_MOTION},
    {"Icon size",
     "Small or native-size banners. Icons are hidden in List view; the previous size is "
     "remembered. Applies to Home, System and folder tabs.",
     true, OPT_ICONS},
    {"Browser view",
     "Index: left/right jumps folders and letters. Shelf: centered covers. Classic: rows. "
     "Extended: details. List: no icons. Grid: tiles. Applies to all tabs.",
     true, OPT_VIEW},
    {"Cover preview",
     "Show selected-item artwork on top. Off uses item information and avoids cover decoding.",
     true, OPT_COVERS},
    {"Clock format",
     "Choose 12 or 24 hours. This affects the launcher display, not the console clock.", true,
     OPT_CLOCK},
    {"Theme",
     "Preview a package and its supported bitmap font. A applies, B cancels. Recovery uses "
     "built-in assets.",
     false, OPT_THEME},
    {"Brightness",
     "Set the console backlight from 1 to 5. On DSi this is a persistent console setting, separate "
     "from theme colors.",
     true, OPT_BRIGHTNESS},
    {"Top wallpaper",
     "Choose a PNG/JPEG anywhere on SD. Personal wallpapers survive theme changes; source images "
     "stay in place.",
     false, OPT_TOP_BACKGROUND},
    {"Bottom wallpaper",
     "Choose a separate image for the touch screen. Required controls and touch targets keep their "
     "positions.",
     false, OPT_BOTTOM_BACKGROUND},
    {"Wallpaper strength",
     "Theme default, Off, or 5-35 percent image strength. Muted backgrounds keep foreground text "
     "readable.",
     true, OPT_WALL_STRENGTH},
    {"Reset wallpapers",
     "Clear personal wallpaper choices and restore the active theme's backgrounds. No images or "
     "theme files are deleted.",
     false, OPT_RESET_WALLPAPERS},
    {"Preset slot",
     "Choose Everyday, Evening or Travel. Each stores your theme, wallpapers, icon/view style and "
     "readability choices.",
     true, OPT_PRESET_SLOT},
    {"Save appearance",
     "Save current appearance into the selected preset. Its previous revision remains as a backup.",
     false, OPT_PRESET_SAVE},
    {"Load appearance",
     "Apply the selected preset. Navigation, game options, associations and saves stay in place.",
     false, OPT_PRESET_LOAD},
    {"Reset appearance",
     "Restore built-in appearance choices. Theme packages and source images remain on SD.", false,
     OPT_RESET_APPEARANCE},
    {"Settings mode",
     "Simple keeps everyday controls. Advanced adds launch, boot and maintenance settings. "
     "Switching views preserves your existing choices.",
     true, OPT_SETTINGS_MODE},
};
static const SettingsOption browser[] = {
    {"Sort order",
     "Folders stay first. Sort files by name, extension or modification date. Selection follows "
     "its path.",
     true, OPT_SORT},
    {"Show hidden",
     "Show dot-hidden files and FAT hidden entries. Navigation dot entries are always "
     "excluded.",
     true, OPT_HIDDEN},
    {"Show system files",
     "Show files marked System by FAT. This does not enable editing or deleting system files.",
     true, OPT_SYSTEM_FILES},
    {"Show extensions",
     "Hide only the displayed filename suffix. Actual paths, associations and saves are unchanged.",
     true, OPT_EXTENSIONS},
    {"File filter",
     "All files, or recognized native, associated and media/document types. Covers remain hidden "
     "as assets.",
     true, OPT_FILTER},
    {"Hide save folders",
     "Hide folders named saves or savestates, ignoring case, in browsers and file pickers. "
     "On by default. Save files remain untouched and game saving still works.",
     true, OPT_HIDE_SAVES},
    {"Hide BIOS folders",
     "Hide folders named bios, ignoring case, in browsers and file pickers. On by default. "
     "Emulators can still read their BIOS files; nothing is moved or deleted.",
     true, OPT_HIDE_BIOS},
    {"Scroll speed",
     "Change held-button repeat speed for browsing and L/R tabs. Single presses remain immediate.",
     true, OPT_SPEED},
    {"Start in",
     "Choose Last tab or a configured tab. This applies at the next startup; it does not launch a "
     "title.",
     true, OPT_STARTUP},
    {"Dashboard",
     "Show, hide, reorder and limit Home sections. L/R adjusts item counts in the editor. Settings "
     "always remains accessible.",
     false, OPT_DASHBOARD},
    {"Title names",
     "Use filenames or lazily loaded internal DS banner titles. Sorting and launch paths still use "
     "the actual filename.",
     true, OPT_TITLE_SOURCE},
    {"Title language",
     "Choose the internal banner language, with English then Japanese fallback. This does not "
     "change the game or translate the UI. Current fonts support ASCII and Latin-1.",
     true, OPT_TITLE_LANGUAGE},
    {"Auto-launch recent",
     "Off by default. At next normal boot, launch the most recent request after a countdown. Any "
     "input cancels; early SELECT recovery bypasses it. Test the title manually first.",
     true, OPT_AUTO_LAUNCH},
};
static const SettingsOption audio[] = {
    {"Background music",
     "Off, the active theme's track, or your custom WAV. Playback stays off in recovery.", true,
     OPT_MUSIC_MODE},
    {"Choose music",
     "Select mono PCM16 WAV at 8-48 kHz, up to 1 MiB. The chosen track loops without another "
     "application.",
     false, OPT_MUSIC_PATH},
    {"Music volume", "Launcher music level, independent of theme effects. Zero mutes playback.",
     true, OPT_MUSIC_VOLUME},
    {"Theme sounds", "Use the active theme's cursor, confirm and back sounds when supplied.", true,
     OPT_EFFECTS},
    {"Sound volume", "Theme UI sound level, independent of background music.", true,
     OPT_EFFECTS_VOLUME},
    {"Playback status",
     "Missing or unsupported optional audio falls back to silence; browsing stays available.",
     false, OPT_AUDIO},
};
static const SettingsOption games[] = {
    {"Emulator profiles",
     "INI profiles select icons and direct homebrew applications. Personal extension defaults can "
     "be edited here.",
     false, OPT_PROFILES},
    {"Homebrew launch",
     "Direct DSi-SD homebrew and nonempty argv are supported. Return needs a compatible "
     "application; see launch documentation.",
     false, OPT_LAUNCH},
    {"Game defaults",
     "Default nds-bootstrap options, save slots and in-game menu keys. Handoff remains "
     "experimental; DSiWare and Slot-1 use separate routes.",
     false, OPT_LOADERS},
    {"Title cheats",
     "Read revision-matched usrcheat.dat entries for the selected title. Selection is separate "
     "from the database. Actual in-game effect still needs validation.",
     false, OPT_CHEATS},
};
static const SettingsOption system_options[] = {
    {"System applications",
     "Open the System tab. Its native launch routes still report their availability separately.",
     false, OPT_SYSTEM},
    {"Return to DSi Menu",
     "Stock-boot Menu return is tested in melonDS. Under Unlaunch, the configured boot target "
     "may open instead. Explicit Unlaunch routing remains experimental.",
     false, OPT_SYSTEM_MENU},
    {"Execution mode",
     "Detected DS/DSi execution mode. This alone does not identify the bootloader or prove "
     "system-title launch support.",
     false, OPT_ENVIRONMENT},
    {"Unlaunch setup",
     "Open your supplied installer to manage health-and-safety, Menu sound and boot setup. "
     "Applying installer choices is separate from frontend preferences.",
     false, OPT_UNLAUNCH},
    {"Battery",
     "Raw hardware battery level (0-15) and charger state. This is not a calculated percentage.",
     false, OPT_BATTERY},
    {"Lid sleep",
     "The SDK's automatic lid sleep service is enabled. Sleep/wake behavior still requires "
     "real-console testing.",
     false, OPT_SLEEP},
};
static const SettingsOption maintenance[] = {
    {"Refresh metadata",
     "Reload themes, associations and this folder; discard prepared artwork thumbnails. Keep "
     "the selected path when it remains visible.",
     false, OPT_REFRESH},
    {"Save preferences",
     "Write frontend settings now. Failed writes remain unsaved and report an error.", false,
     OPT_SAVE},
    {"Build information",
     "BlocksDS frontend. Direct homebrew, bounded audio and native theme packs are implemented. "
     "Retail handoff is experimental.",
     false, OPT_ABOUT},
    {"Folder tabs",
     "Edit _nds/dsi-frontend/tabs.ini for library roots and custom tabs. Restart after editing. "
     "IDs preserve "
     "saved navigation.",
     false, OPT_TABS},
    {"Toolchain",
     "SDK version used for this build. Package details and the SD compatibility override are "
     "documented in docs/development/TOOLCHAIN.md.",
     false, OPT_TOOLCHAIN},
    {"Export diagnostics",
     "Write observed runtime and build state to _nds/dsi-frontend/diagnostics.txt. No BIOS, NAND, "
     "keys or content files are exported.",
     false, OPT_DIAGNOSTICS},
    {"Clear caches",
     "Discard prepared icons, RAM artwork and disposable disk thumbnails. Source images, "
     "assignments and game saves remain on SD.",
     false, OPT_CLEAR_CACHES},
    {"Clear recent list",
     "Clear frontend launch-request history. Favorites, content and saves remain on SD.", false,
     OPT_CLEAR_RECENT},
    {"Reset browser options",
     "Restore sorting, visibility, extension labels, title language and scroll defaults. "
     "Libraries, folder tabs, favorites and appearance stay in place.",
     false, OPT_RESET_BROWSER},
    {"Export configuration",
     "Save frontend settings, tabs, pins, history and item/game/association records in "
     "configuration/export.dsconfig. Apps, artwork and saves are not included.",
     false, OPT_CONFIG_EXPORT},
    {"Import configuration",
     "Inspect a .dsconfig archive, then confirm import. Current records are backed up and settings "
     "reload after successful import. Available outside recovery.",
     false, OPT_CONFIG_IMPORT},
    {"Reset configuration",
     "Restore all frontend settings, tabs, favorites and game/item overrides. A again confirms. "
     "Previous settings are archived; apps, saves, loaders, themes and artwork are kept.",
     false, OPT_RESET_CONFIGURATION},
};
static const SettingsOption routes[] = {
    {"Direct homebrew",
     "DSi SD direct loading. Each app must support argv and loader return. "
     "Press A to refresh installed-route checks.",
     false, OPT_ROUTE_DIRECT},
    {"Retail backend",
     "Checks /_nds/nds-bootstrap-release.nds. Present means installed, "
     "not verified game compatibility. Press A to refresh.",
     false, OPT_ROUTE_RETAIL},
    {"Homebrew backend",
     "Checks /_nds/nds-bootstrap-hb-release.nds for profiles that request it. "
     "Most homebrew uses direct loading. Press A to refresh.",
     false, OPT_ROUTE_HOMEBREW},
    {"Slot-1 helper",
     "Checks supported helper paths and validates the selected homebrew binary. "
     "A cartridge is still required. Press A to refresh.",
     false, OPT_ROUTE_SLOT1},
    {"System Settings",
     "Read-only NAND discovery checks this console's installed title. "
     "A found title does not prove Unlaunch routing. A refreshes discovery.",
     false, OPT_ROUTE_SETTINGS},
    {"Camera",
     "Read-only NAND discovery. Stock-boot controls work; Unlaunch routes still need "
     "validation. A refreshes discovery.",
     false, OPT_ROUTE_CAMERA},
    {"Sound",
     "Discover installed Sound and check its native route. Playback and recording depend on the "
     "boot environment. A refreshes discovery.",
     false, OPT_ROUTE_SOUND},
    {"PictoChat",
     "Discover PictoChat and check its native route. Local wireless needs compatible firmware "
     "and peers. A refreshes discovery.",
     false, OPT_ROUTE_PICTO},
    {"Download Play",
     "Read-only NAND discovery. Stock-boot app startup works; wireless transfer "
     "and Unlaunch routing are unverified. A refreshes discovery.",
     false, OPT_ROUTE_DOWNLOAD},
    {"System Menu",
     "Stock-boot return works in the test setup. Unlaunch may select its configured "
     "boot target instead. This frontend does not change NAND boot settings.",
     false, OPT_ROUTE_MENU},
};
static const struct {
    const SettingsOption *options;
    int count;
} sections[] = {
    {appearance, sizeof appearance / sizeof *appearance},
    {browser, sizeof browser / sizeof *browser},
    {audio, sizeof audio / sizeof *audio},
    {games, sizeof games / sizeof *games},
    {system_options, sizeof system_options / sizeof *system_options},
    {maintenance, sizeof maintenance / sizeof *maintenance},
    {routes, sizeof routes / sizeof *routes},
};

static bool backend_present(const App *app, const char *name)
{
    char path[PATH_CAP];
    struct stat status;
    return app->storage_ready && !app->storage_lost && path_join(path, app->root, name) &&
           !stat(path, &status) && S_ISREG(status.st_mode) && status.st_size >= 512;
}
static void refresh_routes(App *app)
{
    app->settings.retail_backend_present = backend_present(app, "_nds/nds-bootstrap-release.nds");
    app->settings.homebrew_backend_present =
        backend_present(app, "_nds/nds-bootstrap-hb-release.nds");
    LaunchPlan plan;
    char error[160];
    app->settings.slot1_helper_ready = launch_prepare_slot1(app, &plan, error);
    app->system_discovery_requested = app->dsi_mode;
}

/* Simple pages reference the same options and handlers as the full catalog. */
static const int simple_appearance[] = {OPT_SETTINGS_MODE, OPT_THEME,  OPT_BRIGHTNESS,
                                        OPT_ACCENT,        OPT_MOTION, OPT_ICONS,
                                        OPT_VIEW,          OPT_COVERS, OPT_CLOCK};
static const int simple_browser[] = {OPT_SORT,      OPT_EXTENSIONS, OPT_HIDE_SAVES,
                                     OPT_HIDE_BIOS, OPT_SPEED,      OPT_TITLE_SOURCE};
static const int simple_audio[] = {OPT_MUSIC_MODE, OPT_MUSIC_PATH, OPT_MUSIC_VOLUME, OPT_EFFECTS,
                                   OPT_EFFECTS_VOLUME};
static const int simple_console[] = {OPT_BATTERY, OPT_SYSTEM, OPT_SYSTEM_MENU, OPT_SAVE, OPT_ABOUT};
static const struct {
    const char *name;
    const int *ids;
    int count;
} simple_sections[] = {
    {"Appearance", simple_appearance, sizeof simple_appearance / sizeof *simple_appearance},
    {"Browsing", simple_browser, sizeof simple_browser / sizeof *simple_browser},
    {"Audio", simple_audio, sizeof simple_audio / sizeof *simple_audio},
    {"Console", simple_console, sizeof simple_console / sizeof *simple_console},
};

bool settings_is_simple(const App *app)
{
    return app->simple_settings_lock || !app->advanced_settings;
}
int settings_section_count(const App *app)
{
    return settings_is_simple(app) ? sizeof simple_sections / sizeof *simple_sections
                                   : SETTINGS_SECTION_COUNT;
}
const char *settings_section_name(const App *app, int section)
{
    if (section < 0 || section >= settings_section_count(app))
        return "Frontend Settings";
    static const char *keys[] = {"appearance", "browser",     "audio", "games",
                                 "system",     "maintenance", "launch"};
    const char *name =
        settings_is_simple(app) ? simple_sections[section].name : section_names[section];
    char key[48];
    /* Simple pages use their own fixed order; names remain separate from keys. */
    if (settings_is_simple(app))
        snprintf(key, sizeof key, "section.simple_%d", section);
    else
        snprintf(key, sizeof key, "section.%s", keys[section]);
    return ui_translate(&app->language, key, name);
}
int settings_option_count(const App *app, int section)
{
    if (section < 0 || section >= settings_section_count(app))
        return 0;
    if (!settings_is_simple(app))
        return sections[section].count;
    int count = 0;
    for (int i = 0; i < simple_sections[section].count; i++)
        if (!app->simple_settings_lock || simple_sections[section].ids[i] != OPT_SETTINGS_MODE)
            count++;
    return count;
}
const SettingsOption *settings_option(const App *app, int section, int index)
{
    if (index < 0 || index >= settings_option_count(app, section))
        return NULL;
    if (!settings_is_simple(app))
        return &sections[section].options[index];
    for (int i = 0; i < simple_sections[section].count; i++) {
        int id = simple_sections[section].ids[i];
        if (app->simple_settings_lock && id == OPT_SETTINGS_MODE)
            continue;
        if (index--)
            continue;
        for (int page = 0; page < SETTINGS_SECTION_COUNT; page++)
            for (int row = 0; row < sections[page].count; row++)
                if (sections[page].options[row].id == id)
                    return &sections[page].options[row];
    }
    return NULL;
}
void settings_open(App *app)
{
    if (app->rail_focus)
        app_rail_toggle(app);
    app->panel = PANEL_NONE;
    memset(&app->settings, 0, sizeof app->settings);
    app->settings.open = true;
    app->settings.section = 0;
    app->settings.text_started_frame = app->frame_count;
}
void settings_close(App *app)
{
    app->settings.reset_confirm = false;
    app->settings.open = false;
    app_reset_preview(app);
    if (app->preferences_dirty && !app_save(app)) {
        path_copy(app->message,
                  "Preferences remain unsaved. Check SD access and try Save preferences again.",
                  sizeof app->message);
        app_panel(app, PANEL_MESSAGE);
    }
}
void settings_back(App *app)
{
    settings_close(app);
}
void settings_move(App *app, int direction)
{
    SettingsState *state = &app->settings;
    state->text_started_frame = app->frame_count;
    state->reset_confirm = false;
    int count = settings_option_count(app, state->section);
    if (!count)
        return;
    state->selected = (state->selected + direction + count) % count;
    if (state->selected < state->scroll)
        state->scroll = state->selected;
    if (state->selected >= state->scroll + SETTINGS_VISIBLE_ROWS)
        state->scroll = state->selected - SETTINGS_VISIBLE_ROWS + 1;
    state->status[0] = 0;
}
void settings_section(App *app, int direction)
{
    app->settings.text_started_frame = app->frame_count;
    app->settings.reset_confirm = false;
    app->settings.section = (app->settings.section + direction + settings_section_count(app)) %
                            settings_section_count(app);
    app->settings.selected = app->settings.scroll = 0;
    app->settings.status[0] = 0;
    if (app->settings.section == 6)
        refresh_routes(app);
}
void settings_adjust(App *app, int direction)
{
    const SettingsOption *option =
        settings_option(app, app->settings.section, app->settings.selected);
    if (!option || !option->adjustable || !direction)
        return;
    app->settings.text_started_frame = app->frame_count;
    bool refresh = false;
    switch (option->id) {
    case OPT_SETTINGS_MODE:
        if (app->simple_settings_lock)
            return;
        app->advanced_settings = !app->advanced_settings;
        app->settings.section = app->settings.selected = app->settings.scroll = 0;
        break;
    case OPT_AUTO_LAUNCH: {
        static const int delays[] = {0, 3, 5, 10};
        int index = 0;
        for (int i = 0; i < 4; i++)
            if (delays[i] == app->auto_launch_delay)
                index = i;
        app->auto_launch_delay = delays[(index + (direction < 0 ? 3 : 1)) % 4];
        break;
    }
    case OPT_PRESET_SLOT:
        app->settings.preset_slot =
            (app->settings.preset_slot + (direction < 0 ? APPEARANCE_PRESET_COUNT - 1 : 1)) %
            APPEARANCE_PRESET_COUNT;
        return;
    case OPT_WALL_STRENGTH:
        if (app->recovery_mode)
            return;
        if (direction > 0)
            app->wallpaper_strength = app->wallpaper_strength < 0 ? 0
                                      : app->wallpaper_strength >= 35
                                          ? -1
                                          : (app->wallpaper_strength / 5 + 1) * 5;
        else
            app->wallpaper_strength = app->wallpaper_strength < 0 ? 35
                                      : app->wallpaper_strength <= 0
                                          ? -1
                                          : (app->wallpaper_strength - 1) / 5 * 5;
        themes_init(app);
        break;
    case OPT_TITLE_SOURCE:
        app->internal_titles = !app->internal_titles;
        app_reset_preview(app);
        break;
    case OPT_TITLE_LANGUAGE:
        app->title_language = (app->title_language + (direction < 0 ? 7 : 1)) % 8;
        refresh = true;
        break;
    case OPT_BRIGHTNESS: {
        int level = app->brightness >= 1 ? app->brightness : 3;
        level += direction < 0 ? -1 : 1;
        if (level < 1)
            level = 1;
        if (level > 5)
            level = 5;
        app->requested_brightness = level;
        return;
    }
    case OPT_MUSIC_MODE:
        app->audio.mode =
            (app->audio.mode + (direction < 0 ? MUSIC_MODE_COUNT - 1 : 1)) % MUSIC_MODE_COUNT;
        audio_request_reload(app);
        break;
    case OPT_MUSIC_VOLUME:
    case OPT_EFFECTS_VOLUME: {
        int *value =
            option->id == OPT_MUSIC_VOLUME ? &app->audio.music_volume : &app->audio.effects_volume;
        *value += direction < 0 ? -5 : 5;
        if (*value < 0)
            *value = 0;
        if (*value > 100)
            *value = 100;
        break;
    }
    case OPT_EFFECTS:
        app->audio.effects_enabled = !app->audio.effects_enabled;
        audio_request_reload(app);
        break;
    case OPT_ACCENT:
        app_adjust_control(app, QUICK_ACCENT, direction);
        return;
    case OPT_MOTION:
        app_adjust_control(app, QUICK_REDUCED_MOTION, direction);
        return;
    case OPT_ICONS:
        app_adjust_control(app, QUICK_ICON_SIZE, direction);
        return;
    case OPT_VIEW:
        app_adjust_control(app, QUICK_BROWSER_STYLE, direction);
        return;
    case OPT_SPEED:
        app_adjust_control(app, QUICK_SCROLL_SPEED, direction);
        return;
    case OPT_COVERS:
        app->show_covers = !app->show_covers;
        app_reset_preview(app);
        break;
    case OPT_CLOCK:
        app->clock_12_hour = !app->clock_12_hour;
        break;
    case OPT_EXTENSIONS:
        app->show_extensions = !app->show_extensions;
        break;
    case OPT_HIDDEN:
        app->show_hidden = !app->show_hidden;
        refresh = true;
        break;
    case OPT_SYSTEM_FILES:
        app->show_system = !app->show_system;
        refresh = true;
        break;
    case OPT_FILTER:
        app->filter_recognized = !app->filter_recognized;
        refresh = true;
        break;
    case OPT_HIDE_BIOS:
        app->hide_bios_folders = !app->hide_bios_folders;
        refresh = true;
        break;
    case OPT_HIDE_SAVES:
        app->hide_save_folders = !app->hide_save_folders;
        refresh = true;
        break;
    case OPT_SORT:
        app->sort_mode = (app->sort_mode + (direction < 0 ? SORT_COUNT - 1 : 1)) % SORT_COUNT;
        refresh = true;
        break;
    case OPT_STARTUP: {
        int choice = tabs_find(app, app->startup_tab) + 1;
        choice = (choice + (direction < 0 ? app->tab_count : 1)) % (app->tab_count + 1);
        path_copy(app->startup_tab, choice ? app->tabs[choice - 1].id : "", TAB_ID_CAP);
        break;
    }
    default:
        return;
    }
    if (refresh)
        browser_refresh(app);
    app_mark_preferences_dirty(app);
    app->settings.status[0] = 0;
}
void settings_activate(App *app)
{
    app->settings.text_started_frame = app->frame_count;
    const SettingsOption *option =
        settings_option(app, app->settings.section, app->settings.selected);
    if (!option)
        return;
    if (option->id >= OPT_ROUTE_DIRECT && option->id <= OPT_ROUTE_MENU) {
        refresh_routes(app);
        path_copy(app->settings.status, "Route checks refreshed; compatibility is app-specific",
                  sizeof app->settings.status);
        return;
    }
    if (option->adjustable) {
        settings_adjust(app, 1);
        return;
    }
    switch (option->id) {
    case OPT_RESET_CONFIGURATION: {
        if (app->recovery_mode || !app->storage_ready || app->storage_lost) {
            app->settings.reset_confirm = false;
            path_copy(app->settings.status, "Restart normally with SD access to reset",
                      sizeof app->settings.status);
            return;
        }
        if (!app->settings.reset_confirm) {
            app->settings.reset_confirm = true;
            path_copy(app->settings.status, "A again confirms; move or B cancels",
                      sizeof app->settings.status);
            return;
        }
        app->settings.reset_confirm = false;
        char error[160] = "Save failed; settings kept";
        if (app_save(app) && configuration_reset(app->root, error)) {
            app->configuration_reload_requested = true;
            app->settings.open = false;
        }
        path_copy(app->message, error, sizeof app->message);
        path_copy(app->settings.status, error, sizeof app->settings.status);
        return;
    }
    case OPT_CONFIG_EXPORT: {
        char error[160] = "Save failed; check SD access";
        if (!app->recovery_mode && app_save(app))
            configuration_export(app->root, error);
        path_copy(app->settings.status,
                  app->recovery_mode ? "Restart normally to export configuration" : error,
                  sizeof app->settings.status);
        return;
    }
    case OPT_CONFIG_IMPORT:
        if (app->recovery_mode || !app_save(app) || !picker_open_configuration(app))
            path_copy(app->settings.status, "Import unavailable; check SD or restart normally",
                      sizeof app->settings.status);
        return;
    case OPT_DIAGNOSTICS:
        path_copy(app->settings.status,
                  diagnostics_export(app) ? "Saved diagnostics.txt in private folder"
                                          : "Export failed; check SD access",
                  sizeof app->settings.status);
        return;
    case OPT_CLEAR_CACHES:
        icons_reset(app);
        artwork_cache_clear();
        app_reset_preview(app);
        path_copy(app->settings.status,
                  artwork_disk_cache_clear()
                      ? "Prepared icons and thumbnails cleared"
                      : "RAM cleared; disk cache unavailable or partly uncleared",
                  sizeof app->settings.status);
        return;
    case OPT_CLEAR_RECENT:
        path_copy(app->settings.status,
                  dashboard_clear_recent(app) ? "Recent list cleared"
                                              : "Save failed; check SD access",
                  sizeof app->settings.status);
        if (app->category == CATEGORY_HOME)
            browser_refresh(app);
        return;
    case OPT_RESET_BROWSER:
        app->sort_mode = SORT_NAME_ASC;
        app->show_hidden = app->show_system = app->filter_recognized = false;
        app->hide_save_folders = app->hide_bios_folders = true;
        app->show_extensions = true;
        app->internal_titles = false;
        app->title_language = 1;
        app->scroll_speed = SCROLL_NORMAL;
        browser_refresh(app);
        app_mark_preferences_dirty(app);
        path_copy(app->settings.status, "Browser options restored", sizeof app->settings.status);
        return;
    case OPT_PRESET_SAVE:
    case OPT_PRESET_LOAD: {
        bool saved = option->id == OPT_PRESET_SAVE
                         ? appearance_save(app, app->settings.preset_slot)
                         : appearance_load(app, app->settings.preset_slot);
        path_copy(app->settings.status,
                  saved ? (option->id == OPT_PRESET_SAVE ? "Appearance saved" : "Appearance loaded")
                        : "Preset unavailable; check SD or restart outside recovery",
                  sizeof app->settings.status);
        return;
    }
    case OPT_RESET_APPEARANCE:
        appearance_reset(app);
        path_copy(app->settings.status,
                  app->recovery_mode ? "Restart normally to reset appearance"
                                     : "Built-in appearance restored",
                  sizeof app->settings.status);
        return;
    case OPT_MUSIC_PATH:
        if (!picker_open_audio(app))
            path_copy(app->settings.status, "Music picker unavailable",
                      sizeof app->settings.status);
        return;
    case OPT_DASHBOARD:
        dashboard_open(app);
        return;
    case OPT_TOP_BACKGROUND:
    case OPT_BOTTOM_BACKGROUND:
        if (!picker_open_background(app, option->id == OPT_BOTTOM_BACKGROUND))
            path_copy(app->settings.status, "Wallpaper picker unavailable in this session",
                      sizeof app->settings.status);
        return;
    case OPT_RESET_WALLPAPERS:
        if (app->recovery_mode) {
            path_copy(app->settings.status, "Restart normally to edit wallpapers",
                      sizeof app->settings.status);
            return;
        }
        app->custom_backgrounds[0][0] = app->custom_backgrounds[1][0] = 0;
        app->wallpaper_strength = -1;
        themes_init(app);
        app_mark_preferences_dirty(app);
        return;
    case OPT_CHEATS:
        cheats_open(app);
        return;
    case OPT_LOADERS:
        game_editor_open(app, true);
        return;
    case OPT_PROFILES:
        association_editor_open(app);
        return;
    case OPT_THEME:
        themes_open(app);
        return;
    case OPT_UNLAUNCH:
        if (launch_prepare_installer(app, &app->launch, app->message)) {
            settings_close(app);
            app->launch_requested = true;
        } else
            path_copy(app->settings.status, app->message, sizeof app->settings.status);
        return;
    case OPT_SYSTEM:
        settings_close(app);
        app_category(app, CATEGORY_SYSTEM);
        return;
    case OPT_SYSTEM_MENU:
        app_request_system_menu(app);
        return;
    case OPT_REFRESH:
        artwork_disk_cache_clear();
        themes_init(app);
        associations_load(app);
        browser_refresh(app);
        path_copy(app->settings.status, "Metadata refreshed", sizeof app->settings.status);
        break;
    case OPT_SAVE:
        path_copy(app->settings.status,
                  app_save(app) ? "Preferences saved" : "Save failed: check SD access",
                  sizeof app->settings.status);
        break;
    default:
        break;
    }
}
void settings_touch(App *app, int x, int y)
{
    if (x < 6 || x >= 250)
        return;
    if (y >= 22 && y < 37) {
        if (x < 112)
            settings_section(app, -1);
        else if (x >= 152)
            settings_section(app, 1);
        return;
    }
    if (y < SETTINGS_LIST_TOP)
        return;
    int row = (y - SETTINGS_LIST_TOP) / SETTINGS_ROW_HEIGHT;
    int selected = app->settings.scroll + row;
    if (row >= SETTINGS_VISIBLE_ROWS ||
        selected >= settings_option_count(app, app->settings.section))
        return;
    if (selected == app->settings.selected)
        settings_activate(app);
    else {
        app->settings.reset_confirm = false;
        app->settings.selected = selected;
        app->settings.text_started_frame = app->frame_count;
        app->settings.status[0] = 0;
    }
}
/* Translate only built-in values; personal paths, tab labels and theme names stay literal. */
static const char *translated_value(const App *app, const char *value)
{
    static const struct {
        const char *key, *english;
    } values[] = {
        {"value.on", "On"},
        {"value.off", "Off"},
        {"value.auto", "Auto"},
        {"value.enabled", "Enabled"},
        {"value.disabled", "Disabled"},
        {"value.simple", "Simple"},
        {"value.advanced", "Advanced"},
        {"value.hidden", "Hidden"},
        {"value.large", "Large"},
        {"value.small", "Small"},
        {"value.classic", "Classic"},
        {"value.extended", "Extended"},
        {"value.list", "List"},
        {"value.grid", "Grid"},
        {"value.shelf", "Shelf"},
        {"value.index", "Index"},
        {"value.theme", "Theme"},
        {"value.custom", "Custom"},
        {"value.hour12", "12 hour"},
        {"value.hour24", "24 hour"},
        {"value.name_asc", "Name A-Z"},
        {"value.name_desc", "Name Z-A"},
        {"value.type", "Type"},
        {"value.newest", "Newest"},
        {"value.known", "Known types"},
        {"value.all_files", "All files"},
        {"value.slow", "Slow"},
        {"value.normal", "Normal"},
        {"value.fast", "Fast"},
        {"value.last_tab", "Last tab"},
        {"value.customize", "Customize"},
        {"value.open", "Open"},
        {"value.unavailable", "Unavailable"},
        {"value.refresh", "Refresh"},
        {"value.unsaved", "Unsaved"},
        {"value.saved", "Saved"},
        {"value.development", "Development"},
        {"value.apply", "Apply"},
        {"value.confirm", "A confirms"},
        {"value.review", "Review"},
        {"value.write_archive", "Write archive"},
        {"value.choose_archive", "Choose archive"},
        {"value.write_report", "Write report"},
        {"value.restore_theme", "Restore theme"},
        {"value.internal", "Internal"},
        {"value.filename", "Filename"},
        {"value.japanese", "Japanese"},
        {"value.english", "English"},
        {"value.french", "French"},
        {"value.german", "German"},
        {"value.italian", "Italian"},
        {"value.spanish", "Spanish"},
        {"value.chinese", "Chinese"},
        {"value.korean", "Korean"},
        {"value.installer", "Open installer"},
        {"value.select_title", "Select title"},
        {"value.configure", "Configure"},
        {"value.automatic", "Automatic"},
        {"value.choose_wav", "Choose WAV"},
        {"value.ready", "Ready"},
        {"value.sd_unavailable", "SD unavailable"},
        {"value.needs_dsi", "Needs DSi mode"},
        {"value.installed", "Installed"},
        {"value.missing", "Missing/invalid"},
        {"value.not_scanned", "Not scanned"},
        {"value.title_found", "Title found"},
        {"value.boot_dependent", "Boot-dependent"},
        {"value.dsi_mode", "DSi mode"},
        {"value.ds_mode", "DS mode"},
        {"value.dsi_sd", "DSi SD"},
        {"value.pending", "Pending"},
        {"value.host", "Host preview"},
    };
    for (unsigned i = 0; i < sizeof values / sizeof *values; i++)
        if (!strcmp(value, values[i].english))
            return ui_translate(&app->language, values[i].key, value);
    return value;
}
void settings_value(const App *app, const SettingsOption *option, char *out, size_t capacity)
{
    const char *value = "Pending";
    bool translate = true;
    if (!option) {
        if (capacity)
            out[0] = 0;
        return;
    }
    switch (option->id) {
    case OPT_SETTINGS_MODE:
        value = settings_is_simple(app) ? "Simple" : "Advanced";
        break;
    case OPT_AUTO_LAUNCH:
        if (app->auto_launch_delay)
            snprintf(out, capacity, "%d %s", app->auto_launch_delay,
                     ui_translate(&app->language, "value.seconds", "seconds"));
        else
            path_copy(out, translated_value(app, "Off"), capacity);
        return;
    case OPT_ROUTE_DIRECT:
        value = app->storage_lost || !app->storage_ready ? "SD unavailable"
                : app->dsi_mode                          ? "DSi SD"
                                                         : "Needs DSi mode";
        break;
    case OPT_ROUTE_RETAIL:
    case OPT_ROUTE_HOMEBREW:
    case OPT_ROUTE_SLOT1:
        value = !app->storage_ready || app->storage_lost ? "SD unavailable"
                : (option->id == OPT_ROUTE_RETAIL     ? app->settings.retail_backend_present
                   : option->id == OPT_ROUTE_HOMEBREW ? app->settings.homebrew_backend_present
                                                      : app->settings.slot1_helper_ready)
                    ? "Installed"
                    : "Missing/invalid";
        break;
    case OPT_ROUTE_SETTINGS:
    case OPT_ROUTE_CAMERA:
    case OPT_ROUTE_SOUND:
    case OPT_ROUTE_PICTO:
    case OPT_ROUTE_DOWNLOAD:
        value = !app->dsi_mode                   ? "Needs DSi mode"
                : !app->system_catalog.attempted ? "Not scanned"
                : app->system_catalog.titles[option->id - OPT_ROUTE_SETTINGS].available
                    ? "Title found"
                    : "Unavailable";
        break;
    case OPT_ROUTE_MENU:
        value = app->dsi_mode ? "Boot-dependent" : "Needs DSi mode";
        break;
    case OPT_PRESET_SLOT:
        snprintf(out, capacity, "%s", appearance_preset_name(app->settings.preset_slot));
        return;
    case OPT_PRESET_SAVE:
    case OPT_PRESET_LOAD:
    case OPT_RESET_APPEARANCE:
        value = "Apply";
        break;
    case OPT_RESET_CONFIGURATION:
        value = app->settings.reset_confirm ? "A confirms" : "Review";
        break;
    case OPT_CONFIG_EXPORT:
        value = "Write archive";
        break;
    case OPT_CONFIG_IMPORT:
        value = "Choose archive";
        break;
    case OPT_DIAGNOSTICS:
        value = "Write report";
        break;
    case OPT_CLEAR_CACHES:
    case OPT_CLEAR_RECENT:
    case OPT_RESET_BROWSER:
        value = "Apply";
        break;
    case OPT_TOP_BACKGROUND:
    case OPT_BOTTOM_BACKGROUND:
        value = *app->custom_backgrounds[option->id == OPT_BOTTOM_BACKGROUND] ? "Custom" : "Theme";
        break;
    case OPT_WALL_STRENGTH:
        if (app->wallpaper_strength < 0)
            value = "Theme";
        else {
            snprintf(out, capacity, "%d%%", app->wallpaper_strength);
            return;
        }
        break;
    case OPT_RESET_WALLPAPERS:
        value = "Restore theme";
        break;
    case OPT_TITLE_SOURCE:
        value = app->internal_titles ? "Internal" : "Filename";
        break;
    case OPT_TITLE_LANGUAGE: {
        static const char *languages[] = {"Japanese", "English", "French",  "German",
                                          "Italian",  "Spanish", "Chinese", "Korean"};
        value = languages[app->title_language];
        break;
    }
    case OPT_UNLAUNCH:
        value = app->dsi_mode ? "Open installer" : "Unavailable";
        break;
    case OPT_CHEATS:
        value = "Select title";
        break;
    case OPT_LOADERS:
        value = "Configure";
        break;
    case OPT_BRIGHTNESS:
        if (app->brightness >= 1)
            snprintf(out, capacity, "%d / 5", app->brightness);
        else
            path_copy(out, translated_value(app, "Unavailable"), capacity);
        return;
    case OPT_BATTERY:
        if (app->battery_level >= 0)
            snprintf(out, capacity, "%d/15%s", app->battery_level,
                     app->battery_charging ? " +AC" : "");
        else
            path_copy(out, translated_value(app, "Unavailable"), capacity);
        return;
    case OPT_SLEEP:
        value = "Automatic";
        break;
    case OPT_MUSIC_MODE: {
        const char *names[] = {"Off", "Theme", "Custom"};
        value = names[app->audio.mode];
        break;
    }
    case OPT_MUSIC_PATH:
        value = *app->audio.custom_path ? app->audio.custom_path : "Choose WAV";
        translate = !*app->audio.custom_path;
        break;
    case OPT_MUSIC_VOLUME:
    case OPT_EFFECTS_VOLUME:
        snprintf(out, capacity, "%d%%",
                 option->id == OPT_MUSIC_VOLUME ? app->audio.music_volume
                                                : app->audio.effects_volume);
        return;
    case OPT_EFFECTS:
        value = app->audio.effects_enabled ? "On" : "Off";
        break;
    case OPT_AUDIO:
        value = *app->audio.status ? app->audio.status : "Ready";
        translate = !*app->audio.status;
        break;
    case OPT_ACCENT: {
        value = *app->themes.active.id && !app->theme_accent_override
                    ? "Theme"
                    : themes_accent_name(app->accent_index);
        break;
    }
    case OPT_MOTION:
        value = app->reduced_motion ? "On" : "Off";
        break;
    case OPT_ICONS:
        value = app->browser_style == BROWSER_LIST ? "Hidden"
                : app->icon_size == ICON_LARGE     ? "Large"
                                                   : "Small";
        break;
    case OPT_VIEW: {
        static const char *names[] = {"Classic", "Extended", "List", "Grid", "Shelf", "Index"};
        value = names[app->browser_style];
        break;
    }
    case OPT_COVERS:
        value = app->show_covers ? "On" : "Off";
        break;
    case OPT_CLOCK:
        value = app->clock_12_hour ? "12 hour" : "24 hour";
        break;
    case OPT_SORT: {
        static const char *names[] = {"Name A-Z", "Name Z-A", "Type", "Newest"};
        value = names[app->sort_mode];
        break;
    }
    case OPT_HIDDEN:
        value = app->show_hidden ? "On" : "Off";
        break;
    case OPT_HIDE_BIOS:
        value = app->hide_bios_folders ? "On" : "Off";
        break;
    case OPT_HIDE_SAVES:
        value = app->hide_save_folders ? "On" : "Off";
        break;
    case OPT_SYSTEM_FILES:
        value = app->show_system ? "On" : "Off";
        break;
    case OPT_EXTENSIONS:
        value = app->show_extensions ? "On" : "Off";
        break;
    case OPT_FILTER:
        value = app->filter_recognized ? "Known types" : "All files";
        break;
    case OPT_SPEED: {
        static const char *names[] = {"Slow", "Normal", "Fast"};
        value = names[app->scroll_speed];
        break;
    }
    case OPT_STARTUP: {
        int tab = tabs_find(app, app->startup_tab);
        value = tab < 0 ? "Last tab" : tabs_label(app, tab);
        translate = tab < 0;
        break;
    }
    case OPT_DASHBOARD:
        value = "Customize";
        break;
    case OPT_THEME:
        value = app->themes.active.name;
        translate = false;
        break;
    case OPT_PROFILES:
        snprintf(out, capacity, "%d %s", app->association_count,
                 ui_translate(&app->language, "value.profiles", "profiles"));
        return;
    case OPT_SYSTEM_MENU:
        value = app->dsi_mode ? "Open" : "Unavailable";
        break;
    case OPT_LAUNCH:
        value = app->dsi_mode && app->storage_ready ? "DSi SD" : "Unavailable";
        break;
    case OPT_SYSTEM:
        value = "Open";
        break;
    case OPT_ENVIRONMENT:
        value = app->dsi_mode ? "DSi mode" : "DS mode";
        break;
    case OPT_REFRESH:
        value = "Refresh";
        break;
    case OPT_SAVE:
        value = app->preferences_dirty ? "Unsaved" : "Saved";
        break;
    case OPT_ABOUT:
        value = "Development";
        break;
    case OPT_TABS:
        snprintf(out, capacity, "%d %s", app->tab_count,
                 ui_translate(&app->language, "value.tabs", "tabs"));
        return;
    case OPT_TOOLCHAIN:
#ifdef HOST_PREVIEW
        value = "Host preview";
#else
        value = BLOCKSDS_VERSION_STRING;
#endif
        break;
    default:
        break;
    }
    path_copy(out, translate ? translated_value(app, value) : value, capacity);
}
void settings_format_clock(char out[12], int hour, int minute, bool twelve_hour)
{
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        path_copy(out, "--:--", 12);
        return;
    }
    if (twelve_hour)
        snprintf(out, 12, "%d:%02d %s", hour % 12 ? hour % 12 : 12, minute,
                 hour < 12 ? "AM" : "PM");
    else
        snprintf(out, 12, "%02d:%02d", hour, minute);
}

const char *settings_option_text(const App *app, const SettingsOption *option, bool help)
{
    char key[48];
    snprintf(key, sizeof key, "setting.%s.%s", option_keys[option->id], help ? "help" : "label");
    return ui_translate(&app->language, key, help ? option->help : option->label);
}
