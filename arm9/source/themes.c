/* Bounded package discovery, validation and transactional preview. */
#include "themes.h"
#include "app.h"
#include "artwork.h"
#include "audio.h"
#include "browser.h"
#include "image_job.h"
#include "paths.h"
#include "theme_assets.h"
#include "theme_composition.h"
#include "theme_motion.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static uint16_t rgb(unsigned value)
{
    return 0x8000 | ((value >> 19) & 31) | ((value >> 6) & 0x3E0) | ((value << 7) & 0x7C00);
}
typedef struct {
    const char *name;
    unsigned bright, dark;
} AccentPreset;
/* Keep the original first three IDs stable in state files and appearance presets. */
static const AccentPreset accents[THEME_ACCENT_COUNT] = {
    {"Amber", 0xEFB357, 0x875400},  {"Mint", 0x6CD3BB, 0x176B59},   {"Iris", 0xAFA3F0, 0x5E438D},
    {"Rose", 0xF08F9C, 0xA4344C},   {"Coral", 0xF39B7B, 0xA64A28},  {"Sky", 0x80BDEE, 0x245D8B},
    {"Cyan", 0x7BDCF2, 0x126678},   {"Lime", 0xA8D875, 0x526A17},   {"Gold", 0xE4CD69, 0x77630A},
    {"Peach", 0xF4BC95, 0x8B542D},  {"Violet", 0xD49BEF, 0x783A92}, {"Pink", 0xF29FCB, 0x91386E},
    {"Teal", 0x64C2C0, 0x126865},   {"Blue", 0x80A7F5, 0x245DAD},   {"Slate", 0xA0AFC2, 0x455464},
    {"Silver", 0xDBE1E6, 0x565E66},
};
static const AccentPreset *accent_preset(int index)
{
    return &accents[(unsigned)index < THEME_ACCENT_COUNT ? index : 0];
}
const char *themes_accent_name(int index)
{
    return accent_preset(index)->name;
}
static uint16_t preset_accent(int index)
{
    return rgb(accent_preset(index)->bright);
}
static void builtin(ThemePackage *package, int accent)
{
    memset(package, 0, sizeof *package);
    path_copy(package->name, "Built-in", sizeof package->name);
    path_copy(package->header, "GLOAM SHELL NDX", sizeof package->header);
    package->palette =
        (ThemePalette){rgb(0x0C1018), rgb(0x151B25),         rgb(0x2A323F), rgb(0xEDEBE1),
                       rgb(0xACB7C6), preset_accent(accent), rgb(0x0D1219)};
    font_builtin(&package->font);
    package->format_version = 1;
    package->screen_opacity[0] = package->screen_opacity[1] = 101;
    package->background_strength = 25;
    package->component_border = 8;
    package->ambient_ticks = 12;
    package->tab_gap = 4;
    package->tab_visible = 3;
    package->scene_cover_x = 14;
    package->scene_cover_y = 34;
    package->scene_cover_width = 48;
    package->scene_cover_height = 72;
    package->default_view = package->default_icon_size = -1;
    package->large_grid_columns = 4;
}
static bool valid_id(const char *id)
{
    if (!*id || strlen(id) >= THEME_ID_CAP)
        return false;
    for (; *id; id++)
        if (!isalnum((unsigned char)*id) && *id != '-' && *id != '_')
            return false;
    return true;
}
static char *trim(char *value)
{
    while (isspace((unsigned char)*value))
        value++;
    size_t length = strlen(value);
    while (length && isspace((unsigned char)value[length - 1]))
        value[--length] = 0;
    return value;
}
static bool color(const char *value, uint16_t *destination)
{
    if (*value++ != '#' || strlen(value) != 6)
        return false;
    for (int i = 0; i < 6; i++)
        if (!isxdigit((unsigned char)value[i]))
            return false;
    *destination = rgb((unsigned)strtoul(value, NULL, 16));
    return true;
}
static int luminance(uint16_t color)
{
    return (3 * (color & 31) + 6 * ((color >> 5) & 31) + ((color >> 10) & 31)) / 10;
}
static bool readable(ThemePalette palette)
{
    int background = luminance(palette.bg), text = luminance(palette.text);
    return abs(text - background) >= 12 && abs(text - luminance(palette.surface)) >= 10 &&
           abs(luminance(palette.muted) - background) >= 6 &&
           abs(luminance(palette.accent) - background) >= 8 &&
           abs(luminance(palette.ink) - luminance(palette.accent)) >= 8;
}
static bool load(App *app, const char *id, ThemePackage *package)
{
    builtin(package, app->accent_index);
    if (!*id) {
        if (!app->recovery_mode && app->wallpaper_strength >= 0)
            package->background_strength = app->wallpaper_strength;
        if (!app->recovery_mode)
            for (int screen = 0; screen < 2; screen++)
                path_copy(package->backgrounds[screen], app->custom_backgrounds[screen], PATH_CAP);
        return true;
    }
    if (!valid_id(id))
        return false;
    char directory[PATH_CAP], relative[128], path[PATH_CAP];
    snprintf(relative, sizeof relative, "themes/%s", id);
    if (!path_preferences_file(directory, app->root, relative) ||
        !path_join(path, directory, "theme.ini"))
        return false;
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    char line[1024], section[32] = "", font[PATH_CAP] = "", heading[PATH_CAP] = "";
    size_t size = 0;
    bool version = false, valid = true, needs_v2 = false;
    while (fgets(line, sizeof line, file)) {
        size += strlen(line);
        if (size > 4096 || (!strchr(line, '\n') && !feof(file))) {
            valid = false;
            break;
        }
        char *key = trim(line);
        if (!*key || *key == ';' || *key == '#')
            continue;
        if (*key == '[') {
            if (strcmp(key, "[theme]") && strcmp(key, "[colors]") && strcmp(key, "[fonts]") &&
                strcmp(key, "[audio]") && strcmp(key, "[backgrounds]") && strcmp(key, "[layout]") &&
                strcmp(key, "[icons]") && strcmp(key, "[components]") && strcmp(key, "[motion]") &&
                strcmp(key, "[assets]"))
                valid = false;
            path_copy(section, key, sizeof section);
            continue;
        }
        char *value = strchr(key, '=');
        if (!value) {
            valid = false;
            continue;
        }
        *value++ = 0;
        key = trim(key);
        value = trim(value);
        if (!strcmp(section, "[theme]")) {
            if (!strcmp(key, "version")) {
                version = !strcmp(value, "1") || !strcmp(value, "2");
                package->format_version = !strcmp(value, "2") ? 2 : 1;
            } else if (!strcmp(key, "header") && strlen(value) < sizeof package->header)
                path_copy(package->header, value, sizeof package->header);
            else if (!strcmp(key, "name") && *value && strlen(value) < sizeof package->name)
                path_copy(package->name, value, sizeof package->name);
            else
                valid = false;
        } else if (!strcmp(section, "[colors]") && !strcmp(key, "selection_text")) {
            valid = color(value, &package->selection_text) && valid;
        } else if (!strcmp(section, "[colors]") && !strcmp(key, "tab_text")) {
            valid = color(value, &package->tab_text) && valid;
        } else if (!strcmp(section, "[colors]") && !strcmp(key, "header_text")) {
            valid = color(value, &package->header_text) && valid;
        } else if (!strcmp(section, "[colors]") && !strcmp(key, "footer_text")) {
            valid = color(value, &package->footer_text) && valid;
        } else if (!strcmp(section, "[colors]") && !strcmp(key, "header_link_text")) {
            valid = color(value, &package->header_link_text) && valid;
        } else if (!strcmp(section, "[colors]")) {
            static const char *names[] = {"background", "surface", "line", "text",
                                          "muted",      "accent",  "ink"};
            uint16_t *colors[] = {&package->palette.bg,    &package->palette.surface,
                                  &package->palette.line,  &package->palette.text,
                                  &package->palette.muted, &package->palette.accent,
                                  &package->palette.ink};
            bool matched = false;
            for (int index = 0; index < 7; index++)
                if (!strcmp(key, names[index])) {
                    matched = true;
                    valid = color(value, colors[index]) && valid;
                }
            if (!matched)
                valid = false;
        } else if (!strcmp(section, "[fonts]") && (!strcmp(key, "ui") || !strcmp(key, "heading"))) {
            bool title = !strcmp(key, "heading");
            needs_v2 |= title;
            if (!*value || strlen(value) >= PATH_CAP)
                valid = false;
            path_copy(title ? heading : font, value, PATH_CAP);
        } else if (!strcmp(section, "[layout]")) {
            if (!strcmp(key, "view")) {
                static const char *names[] = {"classic", "extended", "list",
                                              "grid",    "shelf",    "index"};
                bool matched = false;
                for (int i = 0; i < BROWSER_STYLE_COUNT; i++)
                    if (!strcmp(value, names[i])) {
                        package->default_view = i;
                        matched = true;
                    }
                valid &= matched;
            } else if (!strcmp(key, "icon_size")) {
                if (!strcmp(value, "small"))
                    package->default_icon_size = ICON_SMALL;
                else if (!strcmp(value, "large"))
                    package->default_icon_size = ICON_LARGE;
                else
                    valid = false;
            } else if (!strcmp(key, "top_layout")) {
                const char *layouts[] = {"sidebar",   "sidebar_left", "poster", "scene",
                                         "spotlight", "gallery",      "journal"};
                bool matched = false;
                for (unsigned i = 0; i < TOP_LAYOUT_COUNT; i++)
                    if (!strcmp(value, layouts[i])) {
                        package->top_layout = i;
                        needs_v2 |= i >= TOP_SPOTLIGHT;
                        matched = true;
                    }
                valid &= matched;
            } else if (!strcmp(key, "tabs_position")) {
                needs_v2 = true;
                if (!strcmp(value, "top"))
                    package->tabs_position = 1;
                else if (!strcmp(value, "bottom"))
                    package->tabs_position = 2;
                else
                    valid = false;
            } else if (!strcmp(key, "tab_style") || !strcmp(key, "tab_highlight")) {
                const char *styles[] = {"labels", "icons", "numbered", "pager"};
                const char *highlights[] = {"skin", "underline", "outline"};
                bool style = !strcmp(key, "tab_style"), matched = false;
                for (unsigned i = 0; i < (style ? 4u : 3u); i++)
                    if (!strcmp(value, style ? styles[i] : highlights[i])) {
                        if (style)
                            package->tab_style = i;
                        else
                            package->tab_highlight = i;
                        matched = true;
                    }
                valid &= matched;
            } else if (!strcmp(key, "tab_gap") || !strcmp(key, "tab_visible")) {
                char *end;
                unsigned long number = strtoul(value, &end, 10);
                bool gap = !strcmp(key, "tab_gap");
                if (!*value || *end || number > (gap ? 8u : 5u) || (!gap && number < 3))
                    valid = false;
                else if (gap)
                    package->tab_gap = number;
                else
                    package->tab_visible = number;
            } else if (!strcmp(key, "chrome")) {
                if (!strcmp(value, "minimal"))
                    package->minimal_chrome = true;
                else if (!strcmp(value, "full"))
                    package->minimal_chrome = false;
                else
                    valid = false;
            } else if (!strcmp(key, "scene_cover_x") || !strcmp(key, "scene_cover_y") ||
                       !strcmp(key, "scene_cover_width") || !strcmp(key, "scene_cover_height")) {
                char *end;
                unsigned long number = strtoul(value, &end, 10);
                if (!*value || *end || number > 256)
                    valid = false;
                else if (!strcmp(key, "scene_cover_x"))
                    package->scene_cover_x = number;
                else if (!strcmp(key, "scene_cover_y"))
                    package->scene_cover_y = number;
                else if (!strcmp(key, "scene_cover_width"))
                    package->scene_cover_width = number;
                else
                    package->scene_cover_height = number;
            } else if (!strcmp(key, "content_opacity") || !strcmp(key, "top_content_opacity") ||
                       !strcmp(key, "bottom_content_opacity")) {
                char *end;
                unsigned long number = strtoul(value, &end, 10);
                if (!*value || *end || number > 100)
                    valid = false;
                else if (!strcmp(key, "content_opacity"))
                    package->content_opacity = number;
                else {
                    needs_v2 = true;
                    package->screen_opacity[!strcmp(key, "bottom_content_opacity")] = number;
                }
            } else if (!strcmp(key, "row_padding") || !strcmp(key, "large_grid_columns")) {
                char *end;
                unsigned long number = strtoul(value, &end, 10);
                bool padding = !strcmp(key, "row_padding");
                if (!*value || *end || number > (padding ? 12u : 4u) || (!padding && number < 2))
                    valid = false;
                else if (padding)
                    package->row_padding = number;
                else
                    package->large_grid_columns = number;
            } else
                valid = false;
        } else if (!strcmp(section, "[backgrounds]")) {
            if (!strcmp(key, "strength")) {
                char *end;
                unsigned long strength = strtoul(value, &end, 10);
                if (!*value || *end || strength > 100)
                    valid = false;
                else
                    package->background_strength = (unsigned)strength;
            } else {
                bool surface = !strcmp(key, "top_surface") || !strcmp(key, "bottom_surface");
                int screen = (!strcmp(key, "top") || !strcmp(key, "top_surface"))         ? 0
                             : (!strcmp(key, "bottom") || !strcmp(key, "bottom_surface")) ? 1
                                                                                          : -1;
                if (surface)
                    needs_v2 = true;
                if (screen < 0 || !*value || *value == '/' || strchr(value, ':') ||
                    !path_resolve_sd(path, directory, value))
                    valid = false;
                else
                    path_copy(surface ? package->reading_surfaces[screen]
                                      : package->backgrounds[screen],
                              path, PATH_CAP);
            }
        } else if (!strcmp(section, "[motion]")) {
            needs_v2 = true;
            if (!strcmp(key, "focus")) {
                if (!strcmp(value, "pulse"))
                    package->focus_feedback = true;
                else if (!strcmp(value, "off"))
                    package->focus_feedback = false;
                else
                    valid = false;
            } else if (!strcmp(key, "ambient_ticks")) {
                char *end;
                unsigned long ticks = strtoul(value, &end, 10);
                if (!*value || *end || ticks < 5 || ticks > 120)
                    valid = false;
                else
                    package->ambient_ticks = ticks;
            } else if (strcmp(key, "ambient") || !*value || *value == '/' || strchr(value, ':') ||
                       !path_resolve_sd(path, directory, value))
                valid = false;
            else
                path_copy(package->ambient_path, path, PATH_CAP);
        } else if (!strcmp(section, "[assets]")) {
            needs_v2 = true;
            if (strcmp(key, "prepared") || !*value || *value == '/' || strchr(value, ':') ||
                !path_resolve_sd(path, directory, value))
                valid = false;
            else
                path_copy(package->prepared_assets, path, PATH_CAP);
        } else if (!strcmp(section, "[components]")) {
            if (!strcmp(key, "material") || !strcmp(key, "focus_material")) {
                needs_v2 = true;
                unsigned index = !strcmp(key, "focus_material");
                if (!*value || *value == '/' || strchr(value, ':') ||
                    !path_resolve_sd(path, directory, value))
                    valid = false;
                else
                    path_copy(package->component_material[index], path, PATH_CAP);
            } else if (!strcmp(key, "fill")) {
                needs_v2 = true;
                if (!strcmp(value, "tile"))
                    package->component_tiled = true;
                else if (!strcmp(value, "stretch"))
                    package->component_tiled = false;
                else
                    valid = false;
            } else if (!strcmp(key, "controls")) {
                needs_v2 = true;
                if (!strcmp(value, "on"))
                    package->control_assets = true;
                else if (!strcmp(value, "off"))
                    package->control_assets = false;
                else
                    valid = false;
            } else if (!strcmp(key, "alpha")) {
                needs_v2 = true;
                if (!strcmp(value, "coverage"))
                    package->component_coverage = true;
                else if (!strcmp(value, "binary"))
                    package->component_coverage = false;
                else
                    valid = false;
            } else if (!strcmp(key, "border")) {
                char *end;
                unsigned long border = strtoul(value, &end, 10);
                if (!*value || *end || border > 12)
                    valid = false;
                else
                    package->component_border = (unsigned)border;
            } else if (strcmp(key, "directory") || !*value || *value == '/' || strchr(value, ':') ||
                       !path_resolve_sd(path, directory, value))
                valid = false;
            else
                path_copy(package->component_directory, path, PATH_CAP);
        } else if (!strcmp(section, "[icons]")) {
            if (strcmp(key, "directory") || !*value || *value == '/' || strchr(value, ':') ||
                !path_resolve_sd(path, directory, value))
                valid = false;
            else
                path_copy(package->icon_directory, path, PATH_CAP);
        } else if (!strcmp(section, "[audio]")) {
            char *destination = !strcmp(key, "music")     ? package->music
                                : !strcmp(key, "cursor")  ? package->sounds[0]
                                : !strcmp(key, "confirm") ? package->sounds[1]
                                : !strcmp(key, "back")    ? package->sounds[2]
                                                          : NULL;
            if (!destination || !*value || *value == '/' || strchr(value, ':') ||
                !path_resolve_sd(path, directory, value))
                valid = false;
            else
                path_copy(destination, path, PATH_CAP);
        } else
            valid = false;
    }
    valid = valid && version && (!needs_v2 || package->format_version == 2) && !ferror(file) &&
            readable(package->palette) &&
            (!package->tabs_position || package->top_layout >= TOP_SPOTLIGHT) &&
            package->scene_cover_x >= 2 && package->scene_cover_y >= 26 &&
            package->scene_cover_width >= 8 && package->scene_cover_width <= 96 &&
            package->scene_cover_height >= 8 && package->scene_cover_height <= 90 &&
            package->scene_cover_x + package->scene_cover_width <= 254 &&
            package->scene_cover_y + package->scene_cover_height <= 116;
    fclose(file);
    if (!valid)
        return false;
    if (*font && strcmp(font, "builtin")) {
        if (*font == '/' || strchr(font, ':') || !path_resolve_sd(path, directory, font) ||
            !font_pack_load(&package->font, path))
            return false;
        package->external_font = true;
    }
    if (package->font.height != FONT_HEIGHT ||
        (package->font.proportional && package->format_version != 2))
        return false;
    if (*heading) {
        if (!strcmp(heading, "builtin"))
            font_builtin(&package->heading_font);
        else if (*heading == '/' || strchr(heading, ':') ||
                 !path_resolve_sd(path, directory, heading) ||
                 !font_pack_load(&package->heading_font, path))
            return false;
        package->custom_heading = true;
    }
    if (!app->recovery_mode && app->wallpaper_strength >= 0)
        package->background_strength = app->wallpaper_strength;
    if (!app->recovery_mode)
        for (int screen = 0; screen < 2; screen++)
            if (*app->custom_backgrounds[screen])
                path_copy(package->backgrounds[screen], app->custom_backgrounds[screen], PATH_CAP);
    path_copy(package->id, id, sizeof package->id);
    path_copy(package->directory, directory, PATH_CAP);
    return true;
}
ThemePalette themes_palette(const App *app)
{
    return app->themes.active.palette;
}
void themes_accent_override(App *app)
{
    const AccentPreset *preset = accent_preset(app->accent_index);
    bool light_background = luminance(app->themes.active.palette.bg) >= 16;
    app->themes.active.palette.accent = rgb(light_background ? preset->dark : preset->bright);
    app->themes.active.palette.ink = rgb(light_background ? 0xF6F8FB : 0x0D1219);
    app->theme_accent_override = *app->theme_id != 0;
}
void themes_init(App *app)
{
    themes_motion_reset();
    char id[THEME_ID_CAP];
    path_copy(id, app->theme_id, sizeof id);
    if (app->recovery_mode)
        id[0] = 0;
    if (!load(app, id, &app->themes.active)) {
        builtin(&app->themes.active, app->accent_index);
        path_copy(app->themes.status, "Theme unavailable; using built-in",
                  sizeof app->themes.status);
    }
    if (app->theme_accent_override)
        themes_accent_override(app);
    font_use(&app->themes.active.font);
    themes_icons_cancel();
    themes_components_cancel();
    theme_assets_begin(&app->themes.active);
    themes_background_load(&app->themes.active, false);
    themes_icons_load(&app->themes.active, false);
    themes_components_load(&app->themes.active, false);
    theme_assets_end();
}
static int compare_ids(const void *left, const void *right)
{
    return strcmp(left, right);
}
static bool preview_visible = true;
bool themes_preview_visible(void)
{
    return preview_visible;
}
static void preview_assets_clear(void)
{
    theme_assets_end();
    themes_background_cancel();
    themes_icons_cancel();
    themes_components_cancel();
    preview_visible = true;
}
static void preview_stop(App *app)
{
    if (app->themes.loading) {
        image_job_cancel();
        /* Cancelled checkpoints stop yielding; the decoder unwinds its owned
         * allocations before any bank is freed or another worker can start. */
        image_job_step();
        image_job_task_finish();
        app->themes.loading = false;
    }
    preview_assets_clear();
}
static void preview_load(void *context)
{
    App *app = context;
    ThemeState *state = &app->themes;
    bool valid = load(app, state->ids[state->selected], &state->candidate);
    if (valid && !image_job_cancelled()) {
        theme_assets_begin(&state->candidate);
        themes_background_load(&state->candidate, true);
        if (!image_job_cancelled()) {
            themes_icons_load(&state->candidate, true);
            themes_components_load(&state->candidate, true);
        }
    }
    theme_assets_end();
    state->ready = valid && !image_job_cancelled();
}
static void preview_complete(App *app)
{
    ThemeState *state = &app->themes;
    preview_visible = true;
    path_copy(state->status,
              state->ready ? "Preview ready."
                           : "Invalid package, font or contrast; retained current theme.",
              sizeof state->status);
    font_use(state->ready ? &state->candidate.font : &state->active.font);
    if (!state->ready)
        preview_assets_clear();
}
static void select_preview(App *app)
{
    ThemeState *state = &app->themes;
    preview_stop(app);
    state->ready = state->pending = state->reuse_active = false;
    font_use(&state->active.font);
    if (!strcmp(state->ids[state->selected], state->active.id)) {
        state->candidate = state->active;
        state->ready = state->reuse_active = true;
        path_copy(state->status, "Current theme.", sizeof state->status);
    } else
        path_copy(state->status, "Press X to load this preview.", sizeof state->status);
}
void themes_preview(App *app)
{
    ThemeState *state = &app->themes;
    if (!state->open || state->pending || state->loading || app->storage_lost)
        return;
    preview_stop(app);
    state->ready = state->reuse_active = false;
    font_use(&state->active.font);
    if (image_job_supported()) {
        artwork_cache_cancel_pending();
        state->pending = true;
        path_copy(state->status, "Loading preview...", sizeof state->status);
    } else {
        preview_load(app);
        preview_complete(app);
    }
}
void themes_tick(App *app)
{
    ThemeState *state = &app->themes;
    if (!state->open || app->storage_lost)
        return;
    if (state->loading) {
        image_job_step();
        if (!image_job_task_finish())
            return;
        state->loading = false;
        preview_complete(app);
    } else if (state->pending && !image_job_busy()) {
        state->pending = false;
        preview_visible = false;
        state->loading = image_job_begin_task(preview_load, app);
        if (!state->loading) {
            path_copy(state->status, "Preview unavailable; select another theme.",
                      sizeof state->status);
            preview_visible = true;
        } else if (image_job_task_finish()) {
            state->loading = false;
            preview_complete(app);
        }
    }
}
void themes_open(App *app)
{
    ThemeState *state = &app->themes;
    state->open = true;
    state->count = 1;
    state->selected = state->scroll = 0;
    memset(state->ids, 0, sizeof state->ids);
    char directory[PATH_CAP];
    DIR *folder = path_preferences_file(directory, app->root, "themes") ? opendir(directory) : NULL;
    if (folder) {
        struct dirent *entry;
        while ((entry = readdir(folder))) {
            char path[PATH_CAP], config[PATH_CAP];
            struct stat status;
            if (!valid_id(entry->d_name) || !path_join(path, directory, entry->d_name) ||
                !path_join(config, path, "theme.ini") || stat(config, &status) ||
                !S_ISREG(status.st_mode))
                continue;
            if (state->count < THEME_CAP)
                path_copy(state->ids[state->count++], entry->d_name, THEME_ID_CAP);
            else if (strcmp(entry->d_name, state->ids[state->count - 1]) < 0)
                path_copy(state->ids[state->count - 1], entry->d_name, THEME_ID_CAP);
            qsort(state->ids + 1, state->count - 1, THEME_ID_CAP, compare_ids);
        }
        closedir(folder);
    }
    for (int index = 0; index < state->count; index++)
        if (!strcmp(state->ids[index], state->active.id))
            state->selected = index;
    state->scroll = state->selected >= 7 ? state->selected - 6 : 0;
    select_preview(app);
}
void themes_move(App *app, int direction)
{
    ThemeState *state = &app->themes;
    if (!state->open || !state->count)
        return;
    state->selected = (state->selected + direction + state->count) % state->count;
    if (state->selected < state->scroll)
        state->scroll = state->selected;
    if (state->selected >= state->scroll + 7)
        state->scroll = state->selected - 6;
    select_preview(app);
}
void themes_cancel(App *app)
{
    preview_stop(app);
    app->themes.ready = app->themes.pending = false;
    app->themes.open = false;
    font_use(&app->themes.active.font);
}
void themes_apply(App *app)
{
    if (!app->themes.open)
        return;
    if (!app->themes.ready) {
        path_copy(app->themes.status,
                  app->themes.pending || app->themes.loading
                      ? "Loading preview..."
                      : "Press X to preview before applying.",
                  sizeof app->themes.status);
        return;
    }
    if (app->themes.reuse_active) {
        themes_cancel(app);
        return;
    }
    if (app->recovery_mode) {
        path_copy(app->themes.status, "Recovery session: restart to apply themes.",
                  sizeof app->themes.status);
        return;
    }
    themes_motion_reset();
    app->themes.active = app->themes.candidate;
    themes_background_accept();
    themes_icons_accept();
    themes_components_accept();
    path_copy(app->theme_id, app->themes.active.id, sizeof app->theme_id);
    app->theme_accent_override = false;
    if (app->themes.active.default_view >= 0)
        app->browser_style = app->themes.active.default_view;
    if (app->themes.active.default_icon_size >= 0)
        app->icon_size = app->themes.active.default_icon_size;
    browser_reflow(app);
    themes_cancel(app);
    audio_request_reload(app);
    app_mark_preferences_dirty(app);
}
void themes_touch(App *app, int x, int y)
{
    if (x < 6 || x >= 250 || y < 41 || y >= 167)
        return;
    int index = app->themes.scroll + (y - 41) / 18;
    if (index >= app->themes.count)
        return;
    if (index == app->themes.selected) {
        if (app->themes.ready)
            themes_apply(app);
        else
            themes_preview(app);
    } else
        themes_move(app, index - app->themes.selected);
}
