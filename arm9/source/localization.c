#include "localization.h"
#include "app.h"
#include "browser.h"
#include "paths.h"
#include "script_fonts.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
void localization_load(App *app)
{
    ui_language_load_selected(&app->language, app->root, app->storage_ready && !app->recovery_mode,
                              app->interface_language);
    for (int i = 0; i < CATEGORY_COUNT; i++)
        path_copy(app->tabs[i].label, ui_text(&app->language, (UiString)i), TAB_LABEL_CAP);
    for (int i = 0; i < UI_STRING_COUNT; i++)
        script_fonts_prepare_text(app->language.values[i]);
    for (unsigned i = 0; i < app->language.extra_count; i++)
        script_fonts_prepare_text(app->language.text + app->language.extra[i].offset);
}
static int compare(const void *left, const void *right)
{
    return strcmp(left, right);
}
void localization_scan(App *app)
{
    SettingsState *state = &app->settings;
    state->locale_count = 2;
    strcpy(state->locales[0], "auto");
    strcpy(state->locales[1], "en");
    char directory[PATH_CAP];
    if (!app->storage_ready || app->storage_lost || app->recovery_mode ||
        !path_preferences_file(directory, app->root, "languages"))
        return;
    DIR *dir = opendir(directory);
    if (!dir)
        return;
    struct dirent *entry;
    unsigned visited = 0;
    while (state->locale_count < UI_LOCALE_CAP && visited++ < 256 && (entry = readdir(dir))) {
        size_t length = strlen(entry->d_name);
        if (length < 5 || length >= 36 || strcmp(entry->d_name + length - 4, ".ini"))
            continue;
        char id[32], path[PATH_CAP];
        memcpy(id, entry->d_name, length - 4);
        id[length - 4] = 0;
        struct stat info;
        if (!ui_locale_valid(id) || !strcmp(id, "auto") || !strcmp(id, "en") ||
            !path_join(path, directory, entry->d_name) || stat(path, &info) ||
            !S_ISREG(info.st_mode) || info.st_size <= 0 || info.st_size > 32768)
            continue;
        path_copy(state->locales[state->locale_count++], id, sizeof id);
    }
    closedir(dir);
    qsort(state->locales + 2, state->locale_count - 2, sizeof state->locales[0], compare);
}
void localization_adjust(App *app, int direction)
{
    if (app->recovery_mode) {
        path_copy(app->settings.status, "Language selection is disabled in recovery", 96);
        return;
    }
    SettingsState *state = &app->settings;
    if (state->locale_count < 2)
        localization_scan(app);
    unsigned current = 0;
    for (unsigned i = 0; i < state->locale_count; i++)
        if (!strcmp(state->locales[i], app->interface_language))
            current = i;
    current = (current + (direction < 0 ? state->locale_count - 1 : 1)) % state->locale_count;
    path_copy(app->interface_language, state->locales[current], sizeof app->interface_language);
    localization_load(app);
    app_scan(app);
    app_mark_preferences_dirty(app);
    path_copy(state->status,
              !app->language.loaded && strcmp(app->interface_language, "en") &&
                      strcmp(app->interface_language, "auto")
                  ? "Catalog unavailable; using English"
                  : "Language selected; missing translations use English",
              sizeof state->status);
}
