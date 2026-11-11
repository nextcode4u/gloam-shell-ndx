/* Stable translation keys, independent of themes and title-banner language. */
#include "ui_strings.h"
#include "paths.h"
#include "text.h"
#include <stdio.h>
#include <string.h>
static const struct {
    const char *key, *english;
} strings[UI_STRING_COUNT] = {{"home", "Home"},
                              {"games", "Games"},
                              {"apps", "Apps"},
                              {"media", "Media"},
                              {"system", "System"},
                              {"files", "Files"},
                              {"settings", "Settings"},
                              {"reading_folder", "Reading folder..."},
                              {"browser_hint", "A Open B Back X Tabs Y Info SEL"},
                              {"tab_hint", "L/R tabs"},
                              {"installed_titles", "Installed DSiWare"},
                              {"empty_folder", "No items in this folder"}};
const char *ui_text(const UiLanguage *language, UiString key)
{
    if ((unsigned)key >= UI_STRING_COUNT)
        return "";
    return language->values[key][0] ? language->values[key] : strings[key].english;
}
static bool valid_text(const char *value)
{
    if (!*value || strlen(value) >= 96)
        return false;
    while (*value) {
        const char *start = value;
        uint32_t cp = text_next(&value);
        if (cp < 32 || cp == 127 || ((unsigned char)*start >= 128 && value == start + 1))
            return false;
    }
    return true;
}
void ui_language_load(UiLanguage *language, const char *root, bool enabled)
{
    memset(language, 0, sizeof *language);
    char path[PATH_CAP];
    if (!enabled || !path_preferences_file(path, root, "language.ini"))
        return;
    FILE *file = fopen(path, "r");
    if (!file)
        return;
    UiLanguage candidate = {0};
    char line[256];
    unsigned bytes = 0, seen = 0;
    bool valid = true, version = false;
    while (fgets(line, sizeof line, file)) {
        bytes += strlen(line);
        if (bytes > 4096 || (!strchr(line, '\n') && !feof(file))) {
            valid = false;
            break;
        }
        line[strcspn(line, "\r\n")] = 0;
        if (!*line || *line == '#' || *line == ';')
            continue;
        if (!strcmp(line, "version=1")) {
            if (version)
                valid = false;
            version = true;
            continue;
        }
        char *value = strchr(line, '=');
        if (!value) {
            valid = false;
            break;
        }
        *value++ = 0;
        int key = 0;
        while (key < UI_STRING_COUNT && strcmp(line, strings[key].key))
            key++;
        if (key == UI_STRING_COUNT || (key <= UI_FILES && strlen(value) >= 64) ||
            (seen & (1u << key)) || !valid_text(value)) {
            valid = false;
            break;
        }
        seen |= 1u << key;
        strcpy(candidate.values[key], value);
    }
    valid &= version && !ferror(file);
    fclose(file);
    if (valid) {
        candidate.loaded = true;
        *language = candidate;
    }
}
