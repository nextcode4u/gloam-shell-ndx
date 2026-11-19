/* Optional top-screen content. Preparation is lazy, bounded and read-only. */
#include "top_content.h"
#include "associations.h"
#include "browser.h"
#include "file_types.h"
#include "game_options.h"
#include "icons.h"
#include "item_preferences.h"
#include "paths.h"
#include "script_fonts.h"
#include "text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

const char *top_content_id(unsigned mode)
{
    static const char *ids[] = {"theme",    "context", "neighbors", "folder",
                                "activity", "smart",   "layout"};
    return mode < TOP_CONTENT_COUNT ? ids[mode] : ids[0];
}
const char *top_content_name(unsigned mode)
{
    static const char *names[] = {"Theme",         "Context", "Neighbors",  "Folder overview",
                                  "Home activity", "Smart",   "Layout only"};
    return mode < TOP_CONTENT_COUNT ? names[mode] : names[0];
}
TopContentMode top_content_preference(unsigned mode)
{
    switch (mode) {
    case TOP_CONTENT_NEIGHBORS:
    case TOP_CONTENT_SMART:
        return (TopContentMode)mode;
    case TOP_CONTENT_CONTEXT:
    case TOP_CONTENT_FOLDER:
    case TOP_CONTENT_ACTIVITY:
        return TOP_CONTENT_SMART;
    default:
        return TOP_CONTENT_THEME;
    }
}

TopContentMode top_content_step(unsigned mode, int direction)
{
    static const TopContentMode choices[] = {TOP_CONTENT_THEME, TOP_CONTENT_NEIGHBORS,
                                             TOP_CONTENT_SMART};
    mode = top_content_preference(mode);
    for (unsigned i = 0; i < 3; i++)
        if (choices[i] == mode)
            return choices[(i + (direction < 0 ? 2 : 1)) % 3];
    return TOP_CONTENT_THEME;
}

TopContentMode top_content_mode(const App *app)
{
    if (app->recovery_mode)
        return TOP_CONTENT_THEME;
    unsigned mode = app->top_content ? (unsigned)app->top_content : app->themes.active.top_content;
    if (mode == TOP_CONTENT_ORIGINAL)
        return TOP_CONTENT_THEME;
    if (mode >= TOP_CONTENT_COUNT)
        mode = TOP_CONTENT_THEME;
    if (mode && app->detail_tab)
        return TOP_CONTENT_CONTEXT;
    if (mode == TOP_CONTENT_SMART) {
        const Entry *entry = app_selected(app);
        mode = app->category == CATEGORY_HOME            ? TOP_CONTENT_ACTIVITY
               : !entry || entry->kind == ITEM_DIRECTORY ? TOP_CONTENT_FOLDER
                                                         : TOP_CONTENT_CONTEXT;
    }
    if (mode == TOP_CONTENT_ACTIVITY && app->category != CATEGORY_HOME)
        mode = TOP_CONTENT_CONTEXT;
    return (TopContentMode)mode;
}

static bool metadata_text(const char *value, size_t capacity)
{
    if (strlen(value) >= capacity)
        return false;
    while (*value) {
        const char *start = value;
        unsigned cp = text_next(&value);
        if (cp < 32 || cp == 127 || ((unsigned char)*start >= 128 && value == start + 1))
            return false;
    }
    return true;
}

static void metadata_read(const Entry *entry, TopContentState *state)
{
    char path[PATH_CAP];
    int bytes = entry->kind == ITEM_DIRECTORY
                    ? snprintf(path, sizeof path, "%s/folder.gloam.ini", entry->path)
                    : snprintf(path, sizeof path, "%s.gloam.ini", entry->path);
    if (bytes < 0 || bytes >= PATH_CAP)
        return;
    FILE *file = fopen(path, "rb");
    if (!file)
        return;
    struct stat info;
    bool valid = !fstat(fileno(file), &info) && S_ISREG(info.st_mode) && info.st_size >= 0 &&
                 info.st_size <= 2048;
    char data[2049];
    size_t bytes_read = valid ? fread(data, 1, sizeof data, file) : 0;
    valid &= bytes_read <= 2048 && !memchr(data, 0, bytes_read);
    data[bytes_read < sizeof data ? bytes_read : 0] = 0;
    unsigned seen = 0;
    char line[384];
    char *cursor = data;
    while (valid && *cursor) {
        char *next = strchr(cursor, '\n');
        size_t length = next ? (size_t)(next - cursor) : strlen(cursor);
        if (length >= sizeof line) {
            valid = false;
            break;
        }
        memcpy(line, cursor, length);
        line[length] = 0;
        cursor += length + (next != NULL);
        if (length && line[length - 1] == '\r')
            line[length - 1] = 0;
        if (!*line || *line == '#' || *line == ';')
            continue;
        char *value = strchr(line, '=');
        if (!value) {
            valid = false;
            break;
        }
        *value++ = 0;
        const char *keys[] = {"version", "title", "developer", "genre", "year", "description"};
        char *fields[] = {NULL,         state->title, state->developer,
                          state->genre, state->year,  state->description};
        size_t capacities[] = {0,
                               sizeof state->title,
                               sizeof state->developer,
                               sizeof state->genre,
                               sizeof state->year,
                               sizeof state->description};
        unsigned key;
        for (key = 0; key < 6 && strcmp(line, keys[key]); key++) {
        }
        if (key == 6 || (seen & (1u << key))) {
            valid = false;
            break;
        }
        seen |= 1u << key;
        if (!key)
            valid = !strcmp(value, "1");
        else if (metadata_text(value, capacities[key]))
            path_copy(fields[key], value, capacities[key]);
        else
            valid = false;
        if (key == 4 && *value) {
            valid &= strlen(value) == 4;
            for (const char *c = value; *c; c++)
                valid &= *c >= '0' && *c <= '9';
        }
    }
    valid &= !ferror(file) && (seen & 1u);
    fclose(file);
    state->metadata_present = valid && (seen & ~1u);
    state->metadata_invalid = !valid;
    if (!valid)
        state->title[0] = state->developer[0] = state->genre[0] = state->year[0] =
            state->description[0] = 0;
    if (valid) {
        script_fonts_prepare_text(state->title);
        script_fonts_prepare_text(state->developer);
        script_fonts_prepare_text(state->genre);
        script_fonts_prepare_text(state->description);
    }
}

static void prepare_handler(App *app, const Entry *entry)
{
    TopContentState *state = &app->top;
    if (entry->kind != ITEM_FILE)
        return;
    ItemPreferences item;
    bool personal = item_preferences_load(app->root, entry->path, &item) && *item.application;
    bool built_in =
        icon_is_ds_title(entry->path) || !strcasecmp(path_extension(entry->path), "argv");
    const Association *profile = personal || built_in ? NULL : association_find(app, entry->path);
    const Association *choices[ASSOCIATION_CAP];
    int count = personal || built_in ? 0 : association_choices(app, entry->path, choices);
    const char *application = personal ? item.application : profile ? profile->application : "";
    if (personal || (count == 1 && profile)) {
        struct stat info;
        bool present = !stat(application, &info) && S_ISREG(info.st_mode);
        path_copy(state->handler,
                  profile && *profile->name   ? profile->name
                  : strrchr(application, '/') ? strrchr(application, '/') + 1
                                              : application,
                  sizeof state->handler);
        snprintf(state->route, sizeof state->route, "%s / %s",
                 present ? "App available" : "App missing",
                 profile && (profile->bootstrap || profile->hbargv) ? "bootstrap" : "direct");
    } else if (icon_is_ds_title(entry->path)) {
        path_copy(state->handler, "DS/DSi title", sizeof state->handler);
        path_copy(state->route, "Header selects launch route", sizeof state->route);
    } else if (!strcasecmp(path_extension(entry->path), "argv")) {
        path_copy(state->handler, "ARGV shortcut", sizeof state->handler);
        path_copy(state->route, "Target checked when launched", sizeof state->route);
    } else {
        path_copy(state->handler, count > 1 ? "Choose app on launch" : "No file association",
                  sizeof state->handler);
    }
    script_fonts_prepare_text(state->handler);
}

static void save_summary(App *app, const Entry *entry, int slot)
{
    if (app->info_header_valid && app->info_unit == 3) {
        path_copy(app->top.save, "DSiWare saves checked at launch", sizeof app->top.save);
        return;
    }
    char directory[PATH_CAP], stem[256], candidate[PATH_CAP], relative[320];
    path_copy(directory, entry->path, sizeof directory);
    char *slash = strrchr(directory, '/');
    if (!slash)
        return;
    path_copy(stem, slash + 1, sizeof stem);
    *slash = 0;
    char *dot = strrchr(stem, '.');
    if (dot)
        *dot = 0;
    unsigned found = 0;
    bool conflict = false;
    for (int variant = 0; variant < (slot ? 1 : 3); variant++) {
        if (slot)
            snprintf(relative, sizeof relative, "saves/%s.sav%d", stem, slot);
        else if (variant == 0)
            snprintf(relative, sizeof relative, "%s.sav", stem);
        else if (variant == 1)
            snprintf(relative, sizeof relative, "%s.sav", strrchr(entry->path, '/') + 1);
        else
            snprintf(relative, sizeof relative, "saves/%s.sav", stem);
        struct stat info;
        if (path_join(candidate, directory, relative) && !stat(candidate, &info)) {
            found += S_ISREG(info.st_mode);
            conflict |= !S_ISREG(info.st_mode);
        }
    }
    snprintf(app->top.save, sizeof app->top.save, "Save: %s",
             conflict    ? "path is not a file"
             : found > 1 ? "multiple paths"
             : found     ? "file present"
                         : "not found");
}

static void prepare_options(App *app, const Entry *entry)
{
    if (entry->kind != ITEM_FILE || !icon_is_ds_title(entry->path))
        return;
    GameOptions effective, overrides;
    game_options_effective(app->root, "", &effective);
    bool custom = game_options_read(app->root, entry->path, &overrides);
    unsigned changed = 0;
    if (custom)
        for (int i = 0; i < GAME_OPTION_COUNT; i++)
            if (overrides.values[i] != GAME_INHERIT) {
                effective.values[i] = overrides.values[i];
                changed++;
            }
    snprintf(app->top.options, sizeof app->top.options, "Slot %d / %u overrides",
             effective.values[GAME_SLOT], changed);
    save_summary(app, entry, effective.values[GAME_SLOT]);
}

void top_content_tick(App *app)
{
    if (!top_content_mode(app) || app->selection_idle_frames < 7 || !app->storage_ready ||
        app->storage_lost || app->recovery_mode)
        return;
    TopContentState *state = &app->top;
    const Entry *entry = app_selected(app);
    if (state->stage == 0) {
        for (unsigned i = 0; i < 3; i++)
            state->samples[i] = -1;
        unsigned samples = 0;
        for (int i = 0; i < app->entry_count; i++) {
            const Entry *item = &app->entries[i];
            if (item->kind == ITEM_DIRECTORY)
                state->folders++;
            if (item->kind == ITEM_FILE) {
                state->files++;
                unsigned type = entry_file_type(item).icon;
                if (type < 15)
                    state->types[type]++;
                if (samples < 3)
                    state->samples[samples++] = i;
            }
        }
        if (top_content_mode(app) == TOP_CONTENT_ACTIVITY) {
            samples = 0;
            for (unsigned i = 0; i < 3; i++)
                state->samples[i] = -1;
            /* Only published Home entries: hidden sections remain hidden. */
            for (int i = 0; i < app->entry_count && samples < 3; i++) {
                const Entry *item = &app->entries[i];
                if (*item->path || (item->kind == ITEM_CATEGORY && item->target < 0))
                    state->samples[samples++] = i;
            }
        }
        state->stage++;
    } else if (state->stage == 1) {
        if (entry && (entry->kind == ITEM_FILE || entry->kind == ITEM_DIRECTORY) &&
            path_is_within_root(entry->path, app->root))
            metadata_read(entry, state);
        state->stage++;
    } else if (state->stage == 2) {
        /* Neighbor/folder cards do not display launch or save details. Leave
         * those reads pending until Y (or Smart) actually needs them. */
        if (top_content_mode(app) != TOP_CONTENT_CONTEXT)
            return;
        if (entry)
            prepare_handler(app, entry);
        state->stage++;
    } else if (state->stage == 3) {
        if (top_content_mode(app) != TOP_CONTENT_CONTEXT)
            return;
        if (entry)
            prepare_options(app, entry);
        state->stage++;
    }
}

void top_content_samples(const App *app, int indexes[3])
{
    int selected = app->locations[app->category].selected;
    TopContentMode mode = top_content_mode(app);
    for (int i = 0; i < 3; i++) {
        int index = (mode == TOP_CONTENT_FOLDER || mode == TOP_CONTENT_ACTIVITY)
                        ? (app->top.stage ? app->top.samples[i] : -1)
                        : selected + i - 1;
        indexes[i] = index >= 0 && index < app->entry_count ? index : -1;
    }
}
