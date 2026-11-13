/* State serialization is separate from browsing and hardware startup. */
#include "preferences.h"
#include "browser.h"
#include "paths.h"
#include "system_routes.h"
#include "tabs.h"
#include "themes.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void read_location(App *app, const char *line, bool reset_locations[TAB_CAP])
{
    int category, selected, scroll, path_offset = 0;
    if (sscanf(line, "location=%d,%d,%d,%n", &category, &selected, &scroll, &path_offset) != 3 ||
        path_offset == 0 || category <= CATEGORY_HOME || category >= CATEGORY_COUNT)
        return;

    const char *path = line + path_offset;
    /* Old unrestricted locations must not escape a newly assigned library. */
    if (strlen(path) >= PATH_CAP || !path_is_within_root(path, tabs_browse_root(app, category))) {
        reset_locations[category] = true;
        app->preferences_dirty = true;
        return;
    }
    Location *location = &app->locations[category];
    path_copy(location->path, path, sizeof location->path);
    location->selected = selected < 0 ? 0 : selected;
    location->scroll = scroll < 0 ? 0 : scroll;
}

static void read_tab_location(App *app, const char *line, bool reset_locations[TAB_CAP])
{
    char id[TAB_ID_CAP];
    int selected, scroll, offset = 0;
    if (sscanf(line, "tab_location=%31[^,],%d,%d,%n", id, &selected, &scroll, &offset) != 3 ||
        !offset)
        return;
    int index = tabs_find(app, id);
    if (index < CATEGORY_COUNT)
        return;
    const char *path = line + offset;
    /* A changed folder assignment must not reopen the previous folder tree. */
    if (strlen(path) >= PATH_CAP || !path_is_within_root(path, tabs_browse_root(app, index))) {
        reset_locations[index] = true;
        app->preferences_dirty = true;
        return;
    }
    path_copy(app->locations[index].path, path, PATH_CAP);
    app->locations[index].selected = selected < 0 ? 0 : selected;
    app->locations[index].scroll = scroll < 0 ? 0 : scroll;
}

static bool read_state_file(App *app, const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
        return false;

    if (fseek(file, 0, SEEK_END) || ftell(file) < 0 || ftell(file) > 128 * 1024 ||
        fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return false;
    }
    char line[1024];
    if (!fgets(line, sizeof line, file) || strncmp(line, "version=1", 9) != 0 ||
        (line[9] != '\r' && line[9] != '\n' && line[9] != '\0')) {
        fclose(file);
        return false;
    }

    /* Older state files had the full settings UI; preserve that view on upgrade. */
    app->advanced_settings = true;
    app->simple_settings_lock = false;
    app->updates.enabled = true;
    char active_tab[TAB_ID_CAP] = "";
    bool has_active_tab = false;
    bool reset_locations[TAB_CAP] = {false};
    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\r\n")] = '\0';
        int value;
        if (!strcmp(line, "settings_mode=simple"))
            app->advanced_settings = false;
        else if (!strcmp(line, "settings_mode=advanced"))
            app->advanced_settings = true;
        else if (sscanf(line, "background_updates=%d", &value) == 1)
            app->updates.enabled = value == 1;
        else if (sscanf(line, "simple_settings_lock=%d", &value) == 1)
            app->simple_settings_lock = value == 1;
        else if (!strncmp(line, "selected_path=", 14) && strlen(line + 14) < PATH_CAP &&
                 path_is_within_root(line + 14, app->root))
            path_copy(app->scan_restore_path, line + 14, sizeof app->scan_restore_path);
        else if (!strcmp(line, "auto_launch_delay=0"))
            app->auto_launch_delay = 0;
        else if (!strcmp(line, "auto_launch_delay=3"))
            app->auto_launch_delay = 3;
        else if (!strcmp(line, "auto_launch_delay=5"))
            app->auto_launch_delay = 5;
        else if (!strcmp(line, "auto_launch_delay=10"))
            app->auto_launch_delay = 10;
        else if (sscanf(line, "music_mode=%d", &value) == 1 && value >= MUSIC_OFF &&
                 value < MUSIC_MODE_COUNT)
            app->audio.mode = value;
        else if (sscanf(line, "music_volume=%d", &value) == 1 && value >= 0 && value <= 100)
            app->audio.music_volume = value;
        else if (sscanf(line, "effects_volume=%d", &value) == 1 && value >= 0 && value <= 100)
            app->audio.effects_volume = value;
        else if (sscanf(line, "theme_sounds=%d", &value) == 1)
            app->audio.effects_enabled = value == 1;
        else if (!strncmp(line, "music_path=", 11) && line[11])
            path_resolve_sd(app->audio.custom_path, app->root, line + 11);
        else if (!strncmp(line, "theme_id=", 9) && strlen(line + 9) < THEME_ID_CAP)
            path_copy(app->theme_id, line + 9, THEME_ID_CAP);
        else if (sscanf(line, "installed_open=%d", &value) == 1)
            app->installed_open = value == 1;
        else if (!strncmp(line, "installed_selection=", 20)) {
            unsigned long id = strtoul(line + 20, NULL, 16);
            if (id <= UINT32_MAX)
                app->installed_selected_id = (uint32_t)id;
        } else if (sscanf(line, "theme_accent_override=%d", &value) == 1)
            app->theme_accent_override = value == 1;
        else if (sscanf(line, "theme=%d", &value) == 1 && value >= 0 &&
                 value < THEME_ACCENT_COUNT) {
            app->accent_index = value;
        } else if (sscanf(line, "category=%d", &value) == 1 && value >= CATEGORY_HOME &&
                   value < CATEGORY_COUNT) {
            app->category = value;
        } else if (sscanf(line, "reduced_motion=%d", &value) == 1) {
            app->reduced_motion = value != 0;
        } else if (sscanf(line, "scroll_speed=%d", &value) == 1 && value >= SCROLL_SLOW &&
                   value < SCROLL_SPEED_COUNT) {
            app->scroll_speed = value;
        } else if (sscanf(line, "icon_size=%d", &value) == 1 && value >= 0 &&
                   value < ICON_SIZE_COUNT) {
            app->icon_size = value;
        } else if (sscanf(line, "browser_style=%d", &value) == 1 && value >= 0 &&
                   value < BROWSER_STYLE_COUNT) {
            app->browser_style = value;
        } else if (sscanf(line, "sort_mode=%d", &value) == 1 && value >= 0 && value < SORT_COUNT) {
            app->sort_mode = value;
        } else if (sscanf(line, "show_hidden=%d", &value) == 1)
            app->show_hidden = value != 0;
        else if (sscanf(line, "show_system=%d", &value) == 1)
            app->show_system = value != 0;
        else if (sscanf(line, "hide_bios_folders=%d", &value) == 1)
            app->hide_bios_folders = value != 0;
        else if (sscanf(line, "hide_save_folders=%d", &value) == 1)
            app->hide_save_folders = value != 0;
        else if (!strncmp(line, "top_background=", 15) && line[15])
            path_resolve_sd(app->custom_backgrounds[0], app->root, line + 15);
        else if (!strncmp(line, "bottom_background=", 18) && line[18])
            path_resolve_sd(app->custom_backgrounds[1], app->root, line + 18);
        else if (sscanf(line, "wallpaper_strength=%d", &value) == 1 && value >= -1 && value <= 35)
            app->wallpaper_strength = value;
        else if (sscanf(line, "internal_titles=%d", &value) == 1)
            app->internal_titles = value == 1;
        else if (sscanf(line, "title_language=%d", &value) == 1 && value >= 0 && value < 8)
            app->title_language = value;
        else if (sscanf(line, "show_extensions=%d", &value) == 1)
            app->show_extensions = value != 0;
        else if (sscanf(line, "show_covers=%d", &value) == 1)
            app->show_covers = value != 0;
        else if (sscanf(line, "filter_recognized=%d", &value) == 1)
            app->filter_recognized = value != 0;
        else if (sscanf(line, "clock_12_hour=%d", &value) == 1)
            app->clock_12_hour = value != 0;
        else if (!strncmp(line, "startup_tab=", 12) && strlen(line + 12) < TAB_ID_CAP)
            path_copy(app->startup_tab, line + 12, TAB_ID_CAP);
        else if (sscanf(line, "detail=%d", &value) == 1 && value >= 0 && value < 2) {
            app->detail_tab = value;
            memset(app->tab_details, value, sizeof app->tab_details);
        } else if (!strncmp(line, "tab_detail=", 11)) {
            char id[TAB_ID_CAP];
            if (sscanf(line + 11, "%31[^,],%d", id, &value) == 2 && value >= 0 && value < 2) {
                int index = tabs_find(app, id);
                if (index >= 0)
                    app->tab_details[index] = value;
            }
        } else if (strncmp(line, "active_tab=", 11) == 0) {
            has_active_tab = true;
            if (strlen(line + 11) < sizeof active_tab)
                path_copy(active_tab, line + 11, sizeof active_tab);
            else
                active_tab[0] = 0;
        } else if (!strncmp(line, "folder_batch=", 13)) {
            char id[TAB_ID_CAP];
            unsigned batch;
            if (sscanf(line + 13, "%31[^,],%u", id, &batch) == 2 && batch < 4096) {
                int tab = tabs_find(app, id);
                if (tab >= CATEGORY_GAMES && tab != CATEGORY_SYSTEM)
                    app->locations[tab].batch = batch;
            }
        } else if (strncmp(line, "tab_location=", 13) == 0) {
            read_tab_location(app, line, reset_locations);
        } else if (strncmp(line, "location=", 9) == 0) {
            read_location(app, line, reset_locations);
        } else if (strncmp(line, "favorite=", 9) == 0 && app->favorite_count < FAVORITE_CAP) {
            const char *path = line + 9;
            if (strlen(path) < PATH_CAP &&
                (path_is_within_root(path, app->root) || system_destination(path) >= 0))
                path_copy(app->favorites[app->favorite_count++], path, PATH_CAP);
        }
    }
    fclose(file);
    /* Batch records can precede or follow locations in a hand-edited file. */
    for (int tab = 0; tab < app->tab_count; tab++)
        if (reset_locations[tab])
            app->locations[tab].batch = 0;
    if (has_active_tab) {
        int index = tabs_find(app, active_tab);
        app->category = index >= 0 ? index : CATEGORY_HOME;
    }
    return true;
}

void preferences_load(App *app)
{
    char filename[PATH_CAP];
    if (path_preferences_file(filename, app->root, "state.ini") && read_state_file(app, filename))
        return;
    if (path_preferences_file(filename, app->root, "state.bak") && read_state_file(app, filename))
        return;

    /* Keep the first scratch's preferences readable without moving user files. */
    char legacy_directory[PATH_CAP];
    if (path_join(legacy_directory, app->root, FRONTEND_LEGACY_DIRECTORY) &&
        path_join(filename, legacy_directory, "state.ini") && read_state_file(app, filename)) {
        app->preferences_dirty = true;
        app->preferences_idle_frames = 0;
    }
}

static bool save_failed(App *app, const char *operation, int error)
{
    if (!error)
        error = EIO;
    snprintf(app->preferences_error, sizeof app->preferences_error, "Save failed: %s. %s (%d).",
             operation, strerror(error), error);
    app->preferences_dirty = true;
    errno = error;
    return false;
}

bool app_save(App *app)
{
    app->preferences_error[0] = 0;
    if (!app->storage_ready || app->storage_lost)
        return save_failed(app, "SD unavailable", ENODEV);
    if (!path_prepare_preferences_directory(app->root))
        return save_failed(app, "prepare _nds/dsi-frontend", errno);

    char current[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP];
    if (!path_preferences_file(current, app->root, "state.ini") ||
        !path_preferences_file(temporary, app->root, "state.tmp") ||
        !path_preferences_file(backup, app->root, "state.bak"))
        return save_failed(app, "settings path too long", ENAMETOOLONG);

    FILE *file = fopen(temporary, "w");
    if (!file)
        return save_failed(app, "open state.tmp", errno);
    errno = 0;
    fprintf(file,
            "version=1\ntheme=%d\ncategory=%d\nreduced_motion=%d\n"
            "scroll_speed=%d\nicon_size=%d\nbrowser_style=%d\ndetail=%d\n",
            app->recovery_mode ? app->recovery_preferences.accent_index : app->accent_index,
            app->category < CATEGORY_COUNT ? app->category : CATEGORY_HOME,
            app->recovery_mode ? app->recovery_preferences.reduced_motion : app->reduced_motion,
            app->scroll_speed,
            app->recovery_mode ? app->recovery_preferences.icon_size : app->icon_size,
            app->recovery_mode ? app->recovery_preferences.browser_style : app->browser_style,
            app->detail_tab);
    fprintf(file,
            "sort_mode=%d\nshow_hidden=%d\nshow_system=%d\nhide_save_folders=%d\nhide_bios_folders="
            "%d\nshow_extensions=%"
            "d\nshow_covers=%"
            "d\nfilter_recognized=%"
            "d\nclock_12_hour=%d\nstartup_tab=%s\n",
            app->sort_mode, app->show_hidden, app->show_system, app->hide_save_folders,
            app->hide_bios_folders, app->show_extensions,
            app->recovery_mode ? app->recovery_preferences.show_covers : app->show_covers,
            app->filter_recognized,
            app->recovery_mode ? app->recovery_preferences.clock_12_hour : app->clock_12_hour,
            app->startup_tab);
    fprintf(file, "background_updates=%d\n", app->updates.enabled);
    fprintf(file, "settings_mode=%s\nsimple_settings_lock=%d\n",
            app->advanced_settings ? "advanced" : "simple", app->simple_settings_lock);
    char music_reference[PATH_CAP] = "";
    if (*app->audio.custom_path && path_is_within_root(app->audio.custom_path, app->root)) {
        const char *relative = app->audio.custom_path + strlen(app->root);
        while (*relative == '/')
            relative++;
        path_resolve_sd(music_reference, "sd:/", relative);
    }
    fprintf(file,
            "music_mode=%d\nmusic_volume=%d\neffects_volume=%d\ntheme_sounds=%d\nmusic_path=%s\n",
            app->audio.mode, app->audio.music_volume, app->audio.effects_volume,
            app->audio.effects_enabled, music_reference);
    fprintf(file, "theme_id=%s\ntheme_accent_override=%d\n", app->theme_id,
            app->recovery_mode ? app->recovery_preferences.accent_override
                               : app->theme_accent_override);
    for (int screen = 0; screen < 2; screen++) {
        char reference[PATH_CAP] = "";
        const char *path = app->custom_backgrounds[screen];
        if (*path && path_is_within_root(path, app->root)) {
            const char *relative = path + strlen(app->root);
            while (*relative == '/')
                relative++;
            path_resolve_sd(reference, "sd:/", relative);
        }
        fprintf(file, "%s_background=%s\n", screen ? "bottom" : "top", reference);
    }
    fprintf(file, "wallpaper_strength=%d\nauto_launch_delay=%d\n", app->wallpaper_strength,
            app->auto_launch_delay);
    fprintf(file, "internal_titles=%d\ntitle_language=%d\n", app->internal_titles,
            app->title_language);
    fprintf(file, "active_tab=%s\n", app->tabs[app->category].id);
    const Entry *selected = app_selected(app);
    if (selected && path_is_within_root(selected->path, app->root) &&
        !strpbrk(selected->path, "\r\n"))
        fprintf(file, "selected_path=%s\n", selected->path);
    for (int index = 0; index < app->tab_count; index++)
        fprintf(file, "tab_detail=%s,%d\n", app->tabs[index].id,
                index == app->category ? app->detail_tab : app->tab_details[index]);
    for (int category = 1; category < CATEGORY_COUNT; category++) {
        const Location *location = &app->locations[category];
        fprintf(file, "location=%d,%d,%d,%s\n", category, location->selected, location->scroll,
                location->path);
    }
    for (int index = CATEGORY_COUNT; index < app->tab_count; index++) {
        const Location *location = &app->locations[index];
        fprintf(file, "tab_location=%s,%d,%d,%s\n", app->tabs[index].id, location->selected,
                location->scroll, location->path);
    }
    for (int index = 1; index < app->tab_count; index++)
        if (index != CATEGORY_SYSTEM && app->locations[index].batch)
            fprintf(file, "folder_batch=%s,%u\n", app->tabs[index].id, app->locations[index].batch);
    for (int index = 0; index < app->favorite_count; index++)
        fprintf(file, "favorite=%s\n", app->favorites[index]);

    fprintf(file, "installed_open=%d\ninstalled_selection=%08lx\n", app->installed_open,
            (unsigned long)app->installed_selected_id);
    int write_error = ferror(file) ? (errno ? errno : EIO) : 0;
    if (fclose(file) != 0 && !write_error)
        write_error = errno ? errno : EIO;
    if (write_error) {
        remove(temporary);
        return save_failed(app, "write state.tmp", write_error);
    }

    /* FAT rename cannot replace a destination. Never continue after a failed
     * backup rotation, or delete a recovery copy when state.ini is absent. */
    struct stat status;
    bool had_current = stat(current, &status) == 0;
    if (!had_current && errno != ENOENT)
        return save_failed(app, "inspect state.ini", errno);
    if (had_current) {
        if (!S_ISREG(status.st_mode))
            return save_failed(app, "state.ini is not a file", EINVAL);
        if (remove(backup) != 0 && errno != ENOENT)
            return save_failed(app, "remove old state.bak", errno);
        if (rename(current, backup) != 0)
            return save_failed(app, "back up state.ini", errno);
    }
    if (rename(temporary, current) != 0) {
        int error = errno;
        if (had_current && rename(backup, current) != 0)
            return save_failed(app, "install state.ini; old settings kept in state.bak", error);
        return save_failed(app, "install state.ini", error);
    }
    app->preferences_dirty = false;
    return true;
}
