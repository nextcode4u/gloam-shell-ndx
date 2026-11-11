/* Application actions and frame-level state updates; no hardware calls. */
#include "app.h"
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
#include "installed_titles.h"
#include "item_preferences.h"
#include "launch.h"
#include "layout.h"
#include "paths.h"
#include "picker.h"
#include "preferences.h"
#include "settings.h"
#include "startup.h"
#include "system_routes.h"
#include "tabs.h"
#include "themes.h"
#include "title_metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool app_is_favorite(const App *app, const char *path)
{
    for (int i = 0; i < app->favorite_count; i++)
        if (!strcmp(path, app->favorites[i]))
            return true;
    return false;
}

static void app_show_message(App *app, const char *s)
{
    path_copy(app->message, s, sizeof app->message);
    app_panel(app, PANEL_MESSAGE);
}

void app_mark_preferences_dirty(App *app)
{
    app->preferences_dirty = true;
    app->preferences_idle_frames = 0;
}

void app_reset_preview(App *app)
{
    artwork_cache_cancel_pending();
    app->selection_idle_frames = 0;
    app->artwork.ready = false;
    app->artwork_resolved = false;
    app->title[0] = app->code[0] = 0;
}

void app_init_mode(App *app, const char *root, bool storage, bool dsi, bool recovery)
{
    memset(app, 0, sizeof *app);
    path_copy(app->root, root, PATH_CAP);
    app->battery_level = app->brightness = app->requested_brightness = -1;
    app->scroll_speed = SCROLL_NORMAL;
    app->audio.music_volume = 70;
    app->audio.effects_volume = 70;
    app->audio.effects_enabled = true;
    app->show_extensions = true;
    app->hide_save_folders = app->hide_bios_folders = true;
    app->title_language = 1;
    app->wallpaper_strength = -1;
    app->show_covers = true;
    app->storage_ready = storage;
    app->storage_card_present = storage;
    app->system_title_requested = -1;
    app->dsi_mode = dsi;
    char recovery_error[160] = "";
    if (storage && (!configuration_reset_recover(root, recovery_error) ||
                    !configuration_recover(root, recovery_error)))
        app->storage_ready = false;
    ui_language_load(&app->language, root, app->storage_ready && !recovery);
    tabs_load(app);
    if (app->storage_ready) {
        preferences_load(app);
        associations_load(app);
    }
    dashboard_load(app);
    app->recovery_mode = recovery;
    if (recovery) {
        app->recovery_preferences.accent_index = app->accent_index;
        app->recovery_preferences.icon_size = app->icon_size;
        app->recovery_preferences.browser_style = app->browser_style;
        app->recovery_preferences.reduced_motion = app->reduced_motion;
        app->recovery_preferences.accent_override = app->theme_accent_override;
        app->recovery_preferences.show_covers = app->show_covers;
        app->recovery_preferences.clock_12_hour = app->clock_12_hour;
        app->accent_index = 0;
        app->icon_size = ICON_SMALL;
        app->browser_style = BROWSER_CLASSIC;
        app->reduced_motion = true;
        app->show_covers = false;
    }
    artwork_disk_cache_configure(root, app->storage_ready && !recovery);
    themes_init(app);
    audio_request_reload(app);
    /* preferences_load temporarily retains the last active content path here.
     * Scanning resets its pending-selection field, so preserve it across begin. */
    char selected_path[PATH_CAP];
    path_copy(selected_path, app->scan_restore_path, sizeof selected_path);
    int saved_category = app->category;
    int startup = recovery ? CATEGORY_HOME : tabs_find(app, app->startup_tab);
    if (startup >= 0)
        app->category = startup;
    app->detail_tab = app->tab_details[app->category];
    tabs_ensure_visible(app);
    app_scan(app);
    if (!recovery && app->category == saved_category)
        browser_restore_selection(app, selected_path);
    if (*recovery_error)
        app_show_message(app, recovery_error);
    else if (*app->themes.status)
        app_show_message(app, app->themes.status);
    startup_prepare(app);
    if (app->tab_config_skipped) {
        snprintf(app->message, sizeof app->message,
                 "Tab configuration: %u invalid or excess entries skipped. Check tabs.ini. "
                 "Built-in tabs remain available.",
                 app->tab_config_skipped);
        app_panel(app, PANEL_MESSAGE);
    }
}

void app_init(App *app, const char *root, bool storage, bool dsi)
{
    app_init_mode(app, root, storage, dsi, false);
}

void app_category(App *app, int c)
{
    app->tab_text_started_frame = app->frame_count;
    app->rail_focus = false;
    app->settings.open = false;
    app->tab_details[app->category] = app->detail_tab;
    app->category = (c % app->tab_count + app->tab_count) % app->tab_count;
    app->detail_tab = app->tab_details[app->category];
    tabs_ensure_visible(app);
    app->panel = PANEL_NONE;
    app_scan(app);
    app_mark_preferences_dirty(app);
}

void app_toggle_details(App *app)
{
    app->detail_tab ^= 1;
    app->tab_details[app->category] = app->detail_tab;
    app_mark_preferences_dirty(app);
}

void app_rail_toggle(App *app)
{
    app->tab_text_started_frame = app->frame_count;
    if (app->rail_focus) {
        app->rail_focus = false;
        app->tab_scroll = app->rail_saved_scroll;
        return;
    }
    app->rail_focus = true;
    app->rail_selected = app->category;
    app->rail_saved_scroll = app->tab_scroll;
}
void app_rail_move(App *app, int direction)
{
    if (!app->rail_focus || !app->tab_count)
        return;
    app->tab_text_started_frame = app->frame_count;
    app->rail_selected = (app->rail_selected + direction + app->tab_count) % app->tab_count;
    if (app->rail_selected < app->tab_scroll)
        app->tab_scroll = app->rail_selected;
    if (app->rail_selected >= app->tab_scroll + TAB_VISIBLE_COUNT)
        app->tab_scroll = app->rail_selected - TAB_VISIBLE_COUNT + 1;
}
void app_rail_commit(App *app)
{
    if (app->rail_focus) {
        int destination = app->rail_selected;
        app->rail_focus = false;
        app_category(app, destination);
    }
}

void app_move(App *app, int delta)
{
    if (!app->entry_count)
        return;
    Location *location = &app->locations[app->category];
    int old = location->scroll;
    location->selected =
        ((location->selected + delta) % app->entry_count + app->entry_count) % app->entry_count;
    if (app->category == CATEGORY_SYSTEM && app->installed_open &&
        app->entries[location->selected].kind == ITEM_INSTALLED_TITLE)
        app->installed_selected_id =
            app->installed_catalog.titles[app->entries[location->selected].target].title.low;
    if (layout_is_grid(app)) {
        browser_reflow(app);
        app->icons_idle_frames = 0;
        app_mark_preferences_dirty(app);
        return;
    }
    if (location->selected < location->scroll)
        location->scroll = location->selected;
    if (location->selected >= location->scroll + layout_visible_rows(app))
        location->scroll = location->selected - layout_visible_rows(app) + 1;
    if (location->scroll != old)
        app->selection_y_fixed =
            (location->selected - location->scroll) * layout_row_height(app) * 256;
    app->icons_idle_frames = 0;
    app_reset_preview(app);
    app_mark_preferences_dirty(app);
}

void app_page(App *app, int direction)
{
    if (!app->entry_count)
        return;
    int current = app->locations[app->category].selected;
    int target = current + (direction < 0 ? -1 : 1) * layout_visible_rows(app);
    if (target < 0)
        target = 0;
    if (target >= app->entry_count)
        target = app->entry_count - 1;
    if (target != current)
        app_move(app, target - current);
}

void app_panel(App *app, Panel p)
{
    /* Editors/pickers retain their underlying menu. Info and message overlays
     * replace it, so remember the item-menu row until that overlay is dismissed. */
    if (p == PANEL_INFO || p == PANEL_MESSAGE) {
        if (app->panel == PANEL_ACTIONS) {
            app->panel_parent = PANEL_ACTIONS;
            app->panel_parent_selection = app->panel_selection;
            app->panel_parent_scroll = app->panel_scroll;
        } else if (app->panel != PANEL_INFO && app->panel != PANEL_MESSAGE)
            app->panel_parent = PANEL_NONE;
    } else
        app->panel_parent = PANEL_NONE;
    if (p == PANEL_ACTIONS) {
        const Entry *entry = app_selected(app);
        bool item = entry && *entry->path && app->storage_ready &&
                    entry->kind != ITEM_INSTALLED_TITLE && entry->kind != ITEM_INSTALLED_LIBRARY;
        bool file = item && entry->kind == ITEM_FILE;
        ItemPreferences preferences = {0};
        if (item)
            item_preferences_load(app->root, entry->path, &preferences);
        app->action_enabled[0] = entry != NULL;
        app->action_enabled[1] =
            item || (entry && entry->kind == ITEM_SYSTEM && system_destination(entry->path) >= 0 &&
                     app->storage_ready);
        app->action_enabled[2] = entry != NULL;
        app->action_enabled[3] = item;
        app->action_enabled[4] = item && *preferences.cover;
        app->action_enabled[5] = file;
        app->action_enabled[6] = file && *preferences.application;
        app->action_enabled[7] = app->storage_ready;
        app->action_enabled[8] = file && icon_is_ds_title(entry->path);
        app->action_enabled[9] = app->action_enabled[8];
        app->action_enabled[14] = item;
        app->action_enabled[15] = item && *preferences.icon;
        app->action_enabled[12] = app->truncated && !app->scan_directory;
        app->action_enabled[13] = app->category != CATEGORY_HOME &&
                                  app->locations[app->category].batch && !app->scan_directory;
        app->action_enabled[10] = entry && entry->kind == ITEM_MISSING && app->storage_ready;
        app->action_enabled[11] =
            entry && app->category == CATEGORY_HOME && *entry->path && app->storage_ready;
    }
    app->panel = p;
    app->panel_selection = 0;
    app->panel_scroll = 0;
    app->panel_age = 0;
}

void app_panel_close(App *app)
{
    bool return_to_actions = app->panel != PANEL_NONE && app->panel_parent == PANEL_ACTIONS;
    int selected = app->panel_parent_selection, scroll = app->panel_parent_scroll;
    app->panel = app->panel_parent = PANEL_NONE;
    if (return_to_actions) {
        app_panel(app, PANEL_ACTIONS); /* Recheck assignments changed by a picker. */
        app->panel_selection = selected;
        app->panel_scroll = scroll;
    }
}

void app_open(App *app)
{
    const Entry *entry = app_selected(app);
    if (!entry)
        return;
    if (entry->kind == ITEM_INSTALLED_LIBRARY) {
        if (!app->dsi_mode) {
            app_show_message(app, "Installed DSiWare requires DSi mode and readable NAND.");
            return;
        }
        app->installed_open = true;
        app->locations[CATEGORY_SYSTEM].selected = app->locations[CATEGORY_SYSTEM].scroll = 0;
        app_scan(app);
        app_mark_preferences_dirty(app);
        return;
    }
    if (entry->kind == ITEM_INSTALLED_TITLE) {
        unsigned index = (unsigned)entry->target;
        if (!installed_title_revalidate(&app->installed_catalog, index)) {
            app_show_message(app,
                             "Installed title changed or is unavailable. Refresh the library.");
            return;
        }
        app->installed_selected_id = app->installed_catalog.titles[index].title.low;
        /* Requests 0..4 are the built-in apps; installed titles follow. */
        app->system_title_requested = 5 + (int)index;
        return;
    }
    if (entry->kind == ITEM_MISSING) {
        app->panel = PANEL_NONE;
        if (!picker_open(app, PICKER_REPAIR))
            app_show_message(app, "Could not open Locate. Check SD access and available memory.");
        return;
    }
    if (entry->kind == ITEM_CATEGORY) {
        app_category(app, entry->target);
        return;
    }
    if (entry->kind == ITEM_DIRECTORY) {
        char path[PATH_CAP];
        path_copy(path, entry->path, sizeof path);
        if (app->category == CATEGORY_HOME)
            app->category = CATEGORY_FILES;
        if (!path_is_within_root(path, tabs_browse_root(app, app->category))) {
            app_show_message(app, "Folder outside this tab's root. Browse it through Files.");
            return;
        }
        Location *location = &app->locations[app->category];
        path_copy(location->path, path, PATH_CAP);
        location->selected = location->scroll = 0;
        location->batch = 0;
        app_scan(app);
        app_mark_preferences_dirty(app);
        return;
    }
    if (entry->kind == ITEM_SYSTEM && entry->target == 6) {
        if (app->dsi_mode)
            app->slot1_requested = true;
        else
            app_show_message(app, "Slot-1 launch requires DSi mode.");
        return;
    }
    if (entry->kind == ITEM_SYSTEM && entry->target == 5) {
        app_request_system_menu(app);
        return;
    }
    if (entry->kind == ITEM_SYSTEM) {
        if (!app->system_catalog.attempted && entry->target >= 0 && entry->target < 5) {
            app->system_discovery_requested = true;
            app->system_title_requested = entry->target;
            return;
        }
        if (entry->target >= 0 && entry->target < 5 &&
            app->system_catalog.titles[entry->target].available) {
            app->system_title_requested = entry->target;
        } else {
            app_show_message(app,
                             *app->system_catalog.status
                                 ? app->system_catalog.status
                                 : "This system title was not discovered on the current NAND.");
        }
        return;
    }
    if (entry->kind == ITEM_FILE && picker_open_handlers(app, entry->path))
        return;
    if (launch_prepare(app, entry->path, &app->launch, app->message))
        app->launch_requested = true;
    else {
        diagnostics_record_launch(app, LAUNCH_REPORT_REJECTED, app->message);
        app_panel(app, PANEL_MESSAGE);
    }
}

void app_back(App *app)
{
    if (app->rail_focus) {
        app_rail_toggle(app);
        return;
    }
    if (app->cheats) {
        cheats_close(app);
        return;
    }
    if (app->game_editor.open) {
        game_editor_close(app);
        return;
    }
    if (app->dashboard.open) {
        app->dashboard.open = false;
        return;
    }
    if (app->association_editor.open && !app->picker.open) {
        association_editor_back(app);
        return;
    }
    if (app->themes.open) {
        themes_cancel(app);
        return;
    }
    if (app->picker.open) {
        picker_back(app);
        return;
    }
    if (app->settings.open) {
        settings_back(app);
        return;
    }
    if (app->panel != PANEL_NONE) {
        app_panel_close(app);
        return;
    }
    if (app->category == CATEGORY_HOME)
        return;
    Location *location = &app->locations[app->category];
    if (app->category == CATEGORY_SYSTEM && app->installed_open) {
        installed_catalog_cancel(&app->installed_catalog);
        if (!app->installed_catalog.ready)
            app->installed_catalog.attempted = false;
        app->installed_discovery_requested = false;
        app->installed_open = false;
        location->selected = 7;
        location->scroll = 0;
        app_scan(app);
        app_mark_preferences_dirty(app);
        return;
    }
    if (app->category == CATEGORY_SYSTEM) {
        app_category(app, CATEGORY_HOME);
        return;
    }
    const char *root = tabs_browse_root(app, app->category);
    size_t n = strlen(root), len = strlen(location->path);
    if (!path_is_within_root(location->path, root)) {
        path_copy(location->path, root, PATH_CAP);
    } else {
        while (len > n && location->path[len - 1] == '/')
            location->path[--len] = 0;
        if (len <= n) {
            if (app->category == CATEGORY_FILES)
                app_category(app, CATEGORY_HOME);
            return;
        }
        char *slash = strrchr(location->path, '/');
        if (slash && slash >= location->path + n)
            *slash = 0;
        else
            path_copy(location->path, root, PATH_CAP);
    }
    location->selected = location->scroll = 0;
    location->batch = 0;
    app_scan(app);
    app_mark_preferences_dirty(app);
}

void app_toggle_favorite(App *app)
{
    const Entry *entry = app_selected(app);
    if (!app->storage_ready || !entry || !entry->path[0] || entry->kind == ITEM_INSTALLED_TITLE ||
        entry->kind == ITEM_INSTALLED_LIBRARY)
        return;
    for (int i = 0; i < app->favorite_count; i++)
        if (!strcmp(entry->path, app->favorites[i])) {
            for (int j = i; j < app->favorite_count - 1; j++)
                path_copy(app->favorites[j], app->favorites[j + 1], PATH_CAP);
            app->favorite_count--;
            app_mark_preferences_dirty(app);
            return;
        }
    if (app->favorite_count >= FAVORITE_CAP) {
        app_show_message(app, "Scratch favorite limit reached (64).");
        return;
    }
    path_copy(app->favorites[app->favorite_count++], entry->path, PATH_CAP);
    app_mark_preferences_dirty(app);
}

void app_panel_move(App *app, int delta)
{
    int count = app->panel == PANEL_QUICK     ? QUICK_CONTROL_COUNT
                : app->panel == PANEL_ACTIONS ? ACTION_COUNT
                                              : 1;
    app->panel_selection = (app->panel_selection + delta + count) % count;
    if (app->panel_selection < app->panel_scroll)
        app->panel_scroll = app->panel_selection;
    if (app->panel_selection >= app->panel_scroll + UI_PANEL_VISIBLE_ROWS)
        app->panel_scroll = app->panel_selection - UI_PANEL_VISIBLE_ROWS + 1;
}

/* Horizontal input changes values only; action rows still require activation. */
bool app_adjust_control(App *app, QuickControl control, int direction)
{
    if (direction == 0)
        return false;
    switch (control) {
    case QUICK_ACCENT:
        app->accent_index =
            (app->accent_index + (direction < 0 ? THEME_ACCENT_COUNT - 1 : 1)) % THEME_ACCENT_COUNT;
        break;
    case QUICK_REDUCED_MOTION:
        app->reduced_motion = !app->reduced_motion;
        break;
    case QUICK_SCROLL_SPEED:
        app->scroll_speed =
            (app->scroll_speed + (direction < 0 ? SCROLL_SPEED_COUNT - 1 : 1)) % SCROLL_SPEED_COUNT;
        break;
    case QUICK_ICON_SIZE:
        if (app->browser_style == BROWSER_LIST)
            return false;
        app->icon_size = (app->icon_size + 1) % ICON_SIZE_COUNT;
        browser_reflow(app);
        break;
    case QUICK_BROWSER_STYLE:
        app->browser_style = (app->browser_style + (direction < 0 ? BROWSER_STYLE_COUNT - 1 : 1)) %
                             BROWSER_STYLE_COUNT;
        browser_reflow(app);
        break;
    default:
        return false;
    }
    if (control == QUICK_ACCENT)
        themes_accent_override(app);
    app_mark_preferences_dirty(app);
    return true;
}

void app_panel_adjust(App *app, int direction)
{
    if (app->panel == PANEL_QUICK)
        app_adjust_control(app, app->panel_selection, direction);
}

void app_request_system_menu(App *app)
{
    if (!app->dsi_mode) {
        app_show_message(app, "Nintendo DSi Menu return requires DSi mode.");
        return;
    }
    app->system_menu_requested = true;
}

void app_panel_activate(App *app)
{
    if (app->panel == PANEL_QUICK) {
        switch (app->panel_selection) {
        case QUICK_ACCENT:
        case QUICK_REDUCED_MOTION:
        case QUICK_SCROLL_SPEED:
        case QUICK_ICON_SIZE:
        case QUICK_BROWSER_STYLE:
            app_panel_adjust(app, 1);
            break;
        case QUICK_HOME:
            app_category(app, CATEGORY_HOME);
            break;
        case QUICK_SYSTEM:
            app_category(app, CATEGORY_SYSTEM);
            break;
        case QUICK_SYSTEM_MENU:
            app_request_system_menu(app);
            break;
        case QUICK_SETTINGS:
            settings_open(app);
            break;
        case QUICK_SAVE:
            if (app_save(app)) {
                app_show_message(app, "Preferences saved.");
            } else {
                app_show_message(app, "Could not save preferences. Check SD card access.");
            }
            break;
        }
    } else if (app->panel == PANEL_ACTIONS) {
        if (!app->action_enabled[app->panel_selection]) {
            app_show_message(
                app,
                "This action is unavailable for the selected item or has no assignment to clear.");
            return;
        }
        switch (app->panel_selection) {
        case 0:
            app->panel = PANEL_NONE;
            app_open(app);
            break;
        case 1:
            app_toggle_favorite(app);
            app->panel = PANEL_NONE;
            break;
        case 2:
            app->detail_tab = 1;
            app_panel(app, PANEL_INFO);
            break;
        case 14:
            if (!picker_open(app, PICKER_ICON))
                app_show_message(app, "Choose a file or folder with SD access first.");
            break;
        case 15:
            if (picker_clear_icon(app))
                app_show_message(app,
                                 "Custom browser icon cleared; original image stays in place.");
            else
                app_show_message(app, "Could not clear icon; check SD access.");
            break;
        case 12:
        case 13:
            app->panel = PANEL_NONE;
            browser_batch(app, app->panel_selection == 12 ? 1 : -1);
            break;
        case 10:
            if (!picker_open(app, PICKER_REPAIR))
                app_show_message(app, "Locate unavailable. Check SD access.");
            break;
        case 11: {
            const Entry *entry = app_selected(app);
            char path[PATH_CAP];
            path_copy(path, entry->path, sizeof path);
            if (dashboard_relink(app, path, NULL)) {
                app->panel = PANEL_NONE;
                browser_refresh(app);
            } else
                app_show_message(app,
                                 "Could not save shortcut removal; target files are unchanged.");
            break;
        }
        case 9:
            cheats_open(app);
            break;
        case 8:
            game_editor_open(app, false);
            break;
        case 7:
            association_editor_open(app);
            break;
        case 3:
        case 5:
            if (!picker_open(app, app->panel_selection == 3 ? PICKER_COVER : PICKER_APPLICATION))
                app_show_message(
                    app,
                    "Choose a file or folder with SD access first. Applications require a file.");
            break;
        case 4:
        case 6:
            if (picker_clear_assignment(app, app->panel_selection == 6))
                app_show_message(app, "Assignment cleared. Automatic lookup is active; original "
                                      "files remain in place.");
            else
                app_show_message(app,
                                 "Could not clear this assignment. Check the item and SD access.");
            break;
        }
    } else
        app_panel_close(app);
}

void app_tick(App *app)
{
    app->frame_count++;
    artwork_cache_update();
    startup_tick(app);
    if (app->installed_catalog.scanning && installed_catalog_step(&app->installed_catalog) &&
        app->category == CATEGORY_SYSTEM && app->installed_open)
        app_scan(app);
    if (app->storage_lost || app->cheats || app->game_editor.open || app->themes.open ||
        app->dashboard.open || (app->association_editor.open && !app->picker.open))
        return;
    if (app->picker.open) {
        picker_tick(app);
        return;
    }
    if (app->scan_directory) {
        browser_scan_step(app);
        return;
    }
    if (!app->settings.open) {
        icons_tick(app);
        title_metadata_tick(app);
    }
    app->selection_idle_frames++;
    if (app->panel_age < 24)
        app->panel_age++;
    Location *location = &app->locations[app->category];
    int row = (location->selected - location->scroll) /
              (layout_is_grid(app) ? layout_grid_columns(app) : 1);
    int target = row * layout_row_height(app) * 256;
    if (app->reduced_motion)
        app->selection_y_fixed = target;
    else {
        int d = target - app->selection_y_fixed;
        app->selection_y_fixed += d / 2;
        if (abs(d) < 32)
            app->selection_y_fixed = target;
    }
    if (!app->settings.open && app->show_covers && !app->artwork.ready && !app->artwork_resolved &&
        app->selection_idle_frames >= 7)
        artwork_load(app);
    const Entry *selected_entry = app_selected(app);
    if (app->internal_titles && selected_entry && *selected_entry->internal_title)
        path_copy(app->title, selected_entry->internal_title, sizeof app->title);
    if (app->preferences_dirty && ++app->preferences_idle_frames > 90) {
        app_save(app);
        app->preferences_idle_frames = 0;
    }
}
