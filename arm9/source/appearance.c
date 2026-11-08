/* Small portable appearance records, independent of navigation and game data. */
#include "appearance.h"
#include "app.h"
#include "audio.h"
#include "browser.h"
#include "paths.h"
#include "themes.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

const char *appearance_preset_name(int slot)
{
    static const char *names[] = {"Everyday", "Evening", "Travel"};
    return slot >= 0 && slot < APPEARANCE_PRESET_COUNT ? names[slot] : "Unavailable";
}
static bool preset_path(const App *app, int slot, const char *suffix, char path[PATH_CAP])
{
    char name[64];
    if (slot < 0 || slot >= APPEARANCE_PRESET_COUNT)
        return false;
    snprintf(name, sizeof name, "appearance-%d.%s", slot + 1, suffix);
    return path_preferences_file(path, app->root, name);
}
static bool reference(const App *app, const char *path, char out[PATH_CAP])
{
    out[0] = 0;
    if (!*path)
        return true;
    if (!path_is_within_root(path, app->root))
        return false;
    const char *relative = path + strlen(app->root);
    while (*relative == '/')
        relative++;
    return path_resolve_sd(out, "sd:/", relative);
}
bool appearance_save(App *app, int slot)
{
    char current[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP];
    char top[PATH_CAP], bottom[PATH_CAP];
    if (!app->storage_ready || app->recovery_mode ||
        !reference(app, app->custom_backgrounds[0], top) ||
        !reference(app, app->custom_backgrounds[1], bottom) ||
        !path_prepare_preferences_directory(app->root) || !preset_path(app, slot, "ini", current) ||
        !preset_path(app, slot, "tmp", temporary) || !preset_path(app, slot, "bak", backup))
        return false;
    FILE *file = fopen(temporary, "w");
    if (!file)
        return false;
    fprintf(file,
            "version=1\nname=%s\ntheme=%s\nvalues=%d,%d,%d,%d,%d,%d,%d,%d\n"
            "top=%s\nbottom=%s\n",
            appearance_preset_name(slot), app->theme_id, app->accent_index,
            app->theme_accent_override, app->reduced_motion, app->icon_size, app->browser_style,
            app->show_covers, app->clock_12_hour, app->wallpaper_strength, top, bottom);
    bool written = !ferror(file);
    if (fclose(file))
        written = false;
    if (!written) {
        remove(temporary);
        return false;
    }
    remove(backup);
    bool existed = rename(current, backup) == 0;
    if (rename(temporary, current)) {
        if (existed)
            rename(backup, current);
        return false;
    }
    return true;
}
static bool read_preset(App *app, const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    char line[PATH_CAP + 32], theme[THEME_ID_CAP] = "", backgrounds[2][PATH_CAP] = {{0}};
    int values[8] = {0};
    unsigned seen = 0, lines = 0;
    bool valid = true;
    while (fgets(line, sizeof line, file)) {
        if (++lines > 8 || (!strchr(line, '\n') && !feof(file))) {
            valid = false;
            break;
        }
        line[strcspn(line, "\r\n")] = 0;
        unsigned field = 0;
        if (!strcmp(line, "version=1"))
            field = 1;
        else if (!strncmp(line, "name=", 5) && strlen(line + 5) < 32)
            field = 2;
        else if (!strncmp(line, "theme=", 6) && strlen(line + 6) < sizeof theme) {
            field = 4;
            path_copy(theme, line + 6, sizeof theme);
            for (const char *p = theme; *p; p++)
                valid &= isalnum((unsigned char)*p) || *p == '_' || *p == '-';
        } else if (!strncmp(line, "values=", 7)) {
            int end = 0;
            field = 8;
            valid &=
                sscanf(line + 7, "%d,%d,%d,%d,%d,%d,%d,%d%n", &values[0], &values[1], &values[2],
                       &values[3], &values[4], &values[5], &values[6], &values[7], &end) == 8 &&
                end && !line[7 + end];
            const int maxima[] = {THEME_ACCENT_COUNT - 1,  1, 1, ICON_SIZE_COUNT - 1,
                                  BROWSER_STYLE_COUNT - 1, 1, 1, 35};
            for (int i = 0; i < 8; i++)
                valid &= values[i] >= (i == 7 ? -1 : 0) && values[i] <= maxima[i];
        } else if (!strncmp(line, "top=", 4) || !strncmp(line, "bottom=", 7)) {
            int screen = line[0] == 'b';
            const char *value = line + (screen ? 7 : 4);
            field = screen ? 32 : 16;
            if (*value)
                valid &= path_resolve_sd(backgrounds[screen], app->root, value);
        } else
            valid = false;
        if (seen & field)
            valid = false;
        seen |= field;
    }
    valid &= !ferror(file) && seen == 63;
    fclose(file);
    if (!valid)
        return false;
    /* All validation precedes mutation. Missing optional assets use theme fallback. */
    path_copy(app->theme_id, theme, sizeof app->theme_id);
    memcpy(app->custom_backgrounds, backgrounds, sizeof backgrounds);
    app->accent_index = values[0];
    app->theme_accent_override = values[1];
    app->reduced_motion = values[2];
    app->icon_size = values[3];
    app->browser_style = values[4];
    app->show_covers = values[5];
    app->clock_12_hour = values[6];
    app->wallpaper_strength = values[7];
    themes_init(app);
    audio_request_reload(app);
    browser_reflow(app);
    app_mark_preferences_dirty(app);
    return true;
}
bool appearance_load(App *app, int slot)
{
    char path[PATH_CAP];
    if (!app->storage_ready || app->recovery_mode || !preset_path(app, slot, "ini", path))
        return false;
    if (read_preset(app, path))
        return true;
    return preset_path(app, slot, "bak", path) && read_preset(app, path);
}
void appearance_reset(App *app)
{
    if (app->recovery_mode)
        return;
    app->theme_id[0] = 0;
    app->custom_backgrounds[0][0] = app->custom_backgrounds[1][0] = 0;
    app->wallpaper_strength = -1;
    app->accent_index = 0;
    app->theme_accent_override = app->reduced_motion = false;
    app->icon_size = ICON_SMALL;
    app->browser_style = BROWSER_CLASSIC;
    app->show_covers = true;
    app->clock_12_hour = false;
    themes_init(app);
    audio_request_reload(app);
    browser_reflow(app);
    app_mark_preferences_dirty(app);
}
