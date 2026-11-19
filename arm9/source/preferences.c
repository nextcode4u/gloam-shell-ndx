/* State serialization is separate from browsing and hardware startup. */
#include "preferences.h"
#include "browser.h"
#include "paths.h"
#include "splash.h"
#include "system_routes.h"
#include "tabs.h"
#include "themes.h"
#include "top_content.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Require a complete numeric field and report overflow before narrowing it. */
static bool read_integer(const char **cursor, char delimiter, int *value)
{
    char *end;
    errno = 0;
    long parsed = strtol(*cursor, &end, 10);
    if (end == *cursor || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX)
        return false;
    end += strspn(end, " \t");
    if (*end != delimiter)
        return false;
    *value = (int)parsed;
    *cursor = end + (delimiter != '\0');
    return true;
}

static bool read_setting_integer(const char *line, const char *key, int *value)
{
    size_t length = strlen(key);
    if (strncmp(line, key, length))
        return false;
    const char *cursor = line + length;
    return read_integer(&cursor, '\0', value);
}

static bool read_tab_id(const char **cursor, char id[TAB_ID_CAP])
{
    const char *comma = strchr(*cursor, ',');
    if (!comma || comma == *cursor || (size_t)(comma - *cursor) >= TAB_ID_CAP)
        return false;
    size_t length = (size_t)(comma - *cursor);
    memcpy(id, *cursor, length);
    id[length] = '\0';
    *cursor = comma + 1;
    return true;
}

static void read_location(App *app, const char *line, bool reset_locations[TAB_CAP])
{
    int category, selected, scroll;
    const char *path = line + 9;
    if (!read_integer(&path, ',', &category) || !read_integer(&path, ',', &selected) ||
        !read_integer(&path, ',', &scroll) || category <= CATEGORY_HOME ||
        category >= CATEGORY_COUNT)
        return;

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
    int selected, scroll;
    const char *path = line + 13;
    if (!read_tab_id(&path, id) || !read_integer(&path, ',', &selected) ||
        !read_integer(&path, ',', &scroll))
        return;
    int index = tabs_find(app, id);
    if (index < CATEGORY_COUNT)
        return;
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

    struct stat metadata;
    if (fstat(fileno(file), &metadata) || !S_ISREG(metadata.st_mode) || metadata.st_size < 9 ||
        metadata.st_size > 128 * 1024) {
        fclose(file);
        return false;
    }
    size_t size = (size_t)metadata.st_size;
    char *contents = malloc(size + 1);
    if (!contents) {
        fclose(file);
        return false;
    }
    /* Validate the complete read before changing settings, so a failed primary
     * cannot contaminate defaults or values recovered from state.bak. */
    bool complete = fread(contents, 1, size, file) == size && !ferror(file);
    if (fclose(file))
        complete = false;
    if (!complete || memchr(contents, '\0', size) || memcmp(contents, "version=1", 9) ||
        (size > 9 && contents[9] != '\r' && contents[9] != '\n')) {
        free(contents);
        return false;
    }
    contents[size] = '\0';
    char *cursor = contents + strcspn(contents, "\r\n");
    char line[1024];

    /* Older state files had the full settings UI; preserve that view on upgrade. */
    app->advanced_settings = true;
    app->simple_settings_lock = false;
    app->updates.enabled = true;
    char active_tab[TAB_ID_CAP] = "";
    bool has_active_tab = false;
    bool reset_locations[TAB_CAP] = {false};
    while (*cursor) {
        cursor += strspn(cursor, "\r\n");
        size_t length = strcspn(cursor, "\r\n");
        const char *record = cursor;
        cursor += length;
        /* Skip an oversized record as a whole, never parse its continuation. */
        if (length >= sizeof line)
            continue;
        memcpy(line, record, length);
        line[length] = '\0';
        int value;
        if (!strcmp(line, "settings_mode=simple"))
            app->advanced_settings = false;
        else if (!strcmp(line, "settings_mode=advanced"))
            app->advanced_settings = true;
        else if (read_setting_integer(line, "background_updates=", &value))
            app->updates.enabled = value == 1;
        else if (read_setting_integer(line, "simple_settings_lock=", &value))
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
        else if (read_setting_integer(line, "splash_mode=", &value) && value >= SPLASH_OFF &&
                 value < SPLASH_MODE_COUNT)
            app->splash_mode = value;
        else if (read_setting_integer(line, "splash_once=", &value))
            app->splash_once = value == 1;
        else if (read_setting_integer(line, "splash_sound=", &value))
            app->splash_sound = value == 1;
        else if (!strncmp(line, "splash_path=", 12) && line[12])
            path_resolve_sd(app->splash_path, app->root, line + 12);
        else if (read_setting_integer(line, "music_mode=", &value) && value >= MUSIC_OFF &&
                 value < MUSIC_MODE_COUNT)
            app->audio.mode = value;
        else if (read_setting_integer(line, "music_volume=", &value) && value >= 0 && value <= 100)
            app->audio.music_volume = value;
        else if (read_setting_integer(line, "effects_volume=", &value) && value >= 0 &&
                 value <= 100)
            app->audio.effects_volume = value;
        else if (read_setting_integer(line, "theme_sounds=", &value))
            app->audio.effects_enabled = value == 1;
        else if (!strncmp(line, "music_path=", 11) && line[11])
            path_resolve_sd(app->audio.custom_path, app->root, line + 11);
        else if (!strncmp(line, "theme_id=", 9) && strlen(line + 9) < THEME_ID_CAP)
            path_copy(app->theme_id, line + 9, THEME_ID_CAP);
        else if (read_setting_integer(line, "installed_open=", &value))
            app->installed_open = value == 1;
        else if (!strncmp(line, "installed_selection=", 20)) {
            const char *start = line + 20;
            start += strspn(start, " \t\v\f");
            char *end;
            errno = 0;
            unsigned long id = strtoul(start, &end, 16);
            if (end != start && *start != '-' && errno != ERANGE && id <= UINT32_MAX &&
                !end[strspn(end, " \t")])
                app->installed_selected_id = (uint32_t)id;
        } else if (read_setting_integer(line, "theme_accent_override=", &value))
            app->theme_accent_override = value == 1;
        else if (read_setting_integer(line, "theme=", &value) && value >= 0 &&
                 value < THEME_ACCENT_COUNT) {
            app->accent_index = value;
        } else if (read_setting_integer(line, "category=", &value) && value >= CATEGORY_HOME &&
                   value < CATEGORY_COUNT) {
            app->category = value;
        } else if (read_setting_integer(line, "reduced_motion=", &value)) {
            app->reduced_motion = value != 0;
        } else if (read_setting_integer(line, "scroll_speed=", &value) && value >= SCROLL_SLOW &&
                   value < SCROLL_SPEED_COUNT) {
            app->scroll_speed = value;
        } else if (read_setting_integer(line, "icon_size=", &value) && value >= 0 &&
                   value < ICON_SIZE_COUNT) {
            app->icon_size = value;
        } else if (read_setting_integer(line, "browser_style=", &value) && value >= 0 &&
                   value < BROWSER_STYLE_COUNT) {
            app->browser_style = value;
        } else if (read_setting_integer(line, "top_content=", &value) && value >= 0 &&
                   value < TOP_CONTENT_COUNT) {
            app->top_content = top_content_preference(value);
        } else if (read_setting_integer(line, "sort_mode=", &value) && value >= 0 &&
                   value < SORT_COUNT) {
            app->sort_mode = value;
        } else if (read_setting_integer(line, "show_hidden=", &value))
            app->show_hidden = value != 0;
        else if (read_setting_integer(line, "show_system=", &value))
            app->show_system = value != 0;
        else if (read_setting_integer(line, "hide_bios_folders=", &value))
            app->hide_bios_folders = value != 0;
        else if (read_setting_integer(line, "hide_save_folders=", &value))
            app->hide_save_folders = value != 0;
        else if (!strncmp(line, "top_background=", 15) && line[15])
            path_resolve_sd(app->custom_backgrounds[0], app->root, line + 15);
        else if (!strncmp(line, "bottom_background=", 18) && line[18])
            path_resolve_sd(app->custom_backgrounds[1], app->root, line + 18);
        else if (read_setting_integer(line, "wallpaper_strength=", &value) && value >= -1 &&
                 value <= 35)
            app->wallpaper_strength = value;
        else if (!strncmp(line, "interface_language=", 19) && ui_locale_valid(line + 19))
            path_copy(app->interface_language, line + 19, sizeof app->interface_language);
        else if (read_setting_integer(line, "internal_titles=", &value))
            app->internal_titles = value == 1;
        else if (read_setting_integer(line, "title_language=", &value) && value >= 0 && value < 8)
            app->title_language = value;
        else if (read_setting_integer(line, "show_extensions=", &value))
            app->show_extensions = value != 0;
        else if (read_setting_integer(line, "show_icon_labels=", &value))
            app->show_icon_labels = value != 0;
        else if (read_setting_integer(line, "show_covers=", &value))
            app->show_covers = value != 0;
        else if (read_setting_integer(line, "filter_recognized=", &value))
            app->filter_recognized = value != 0;
        else if (read_setting_integer(line, "clock_12_hour=", &value))
            app->clock_12_hour = value != 0;
        else if (!strncmp(line, "startup_tab=", 12) && strlen(line + 12) < TAB_ID_CAP)
            path_copy(app->startup_tab, line + 12, TAB_ID_CAP);
        else if (read_setting_integer(line, "detail=", &value) && value >= 0 && value < 2) {
            app->detail_tab = value;
            memset(app->tab_details, value, sizeof app->tab_details);
        } else if (!strncmp(line, "tab_display=", 12)) {
            char id[TAB_ID_CAP];
            int view, icons, labels;
            const char *cursor = line + 12;
            if (read_tab_id(&cursor, id) && read_integer(&cursor, ',', &view) &&
                read_integer(&cursor, ',', &icons) && read_integer(&cursor, '\0', &labels) &&
                view >= 0 && view < BROWSER_STYLE_COUNT && icons >= 0 && icons < ICON_SIZE_COUNT &&
                labels >= 0 && labels <= 1) {
                int index = tabs_find(app, id);
                if (index >= 0) {
                    app->tab_display[index].view = view;
                    app->tab_display[index].icons = icons;
                    app->tab_display[index].labels = labels;
                    app->tab_display[index].saved = true;
                }
            }
        } else if (!strncmp(line, "tab_detail=", 11)) {
            char id[TAB_ID_CAP];
            const char *cursor = line + 11;
            if (read_tab_id(&cursor, id) && read_integer(&cursor, '\0', &value) && value >= 0 &&
                value < 2) {
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
            int batch;
            const char *cursor = line + 13;
            if (read_tab_id(&cursor, id) && read_integer(&cursor, '\0', &batch) && batch >= 0 &&
                batch < 4096) {
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
    free(contents);
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
    fprintf(file, "top_content=%d\n", top_content_preference(app->top_content));
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
    fprintf(file, "show_icon_labels=%d\n",
            app->recovery_mode ? app->recovery_preferences.show_icon_labels
                               : app->show_icon_labels);
    char splash_reference[PATH_CAP] = "";
    if (*app->splash_path && path_is_within_root(app->splash_path, app->root)) {
        const char *relative = app->splash_path + strlen(app->root);
        while (*relative == '/')
            relative++;
        path_resolve_sd(splash_reference, "sd:/", relative);
    }
    fprintf(file, "splash_mode=%d\nsplash_once=%d\nsplash_sound=%d\nsplash_path=%s\n",
            app->splash_mode, app->splash_once, app->splash_sound, splash_reference);
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
    fprintf(file, "interface_language=%s\n", app->interface_language);
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
    for (int index = 0; index < app->tab_count; index++) {
        bool active = index == app->category && !app->recovery_mode;
        fprintf(file, "tab_display=%s,%u,%u,%u\n", app->tabs[index].id,
                (unsigned)(active ? app->browser_style : app->tab_display[index].view),
                (unsigned)(active ? app->icon_size : app->tab_display[index].icons),
                (unsigned)(active ? app->show_icon_labels : app->tab_display[index].labels));
    }
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

    FileCommitResult result = path_commit_file(temporary, current, backup);
    static const char *operations[] = {"",
                                       "inspect state.ini",
                                       "remove old state.bak",
                                       "back up state.ini",
                                       "install state.ini",
                                       "install state.ini; old settings kept in state.bak"};
    if (result != FILE_COMMIT_OK)
        return save_failed(app, operations[result], errno);
    app->preferences_dirty = false;
    return true;
}
