/* Stable translation keys, independent of themes and title-banner language. */
#include "ui_strings.h"
#include "paths.h"
#include "text.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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
const char *ui_translate(const UiLanguage *language, const char *key, const char *fallback)
{
    for (unsigned i = 0; i < language->extra_count; i++)
        if (!strcmp(language->extra[i].key, key))
            return language->extra[i].value;
    return fallback;
}
static bool extra_key(const char *key)
{
    if (strlen(key) >= 48 || (strncmp(key, "setting.", 8) && strncmp(key, "section.", 8) &&
                              strncmp(key, "dialog.", 7) && strncmp(key, "value.", 6)))
        return false;
    for (const char *p = key; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '.'))
            return false;
    return true;
}
static bool valid_text(const char *value, size_t capacity)
{
    if (!*value || strlen(value) >= capacity)
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
    UiLanguage *candidate = calloc(1, sizeof *candidate);
    if (!candidate) {
        fclose(file);
        return;
    }
    char line[512];
    unsigned bytes = 0;
    bool seen[UI_STRING_COUNT] = {0};
    bool valid = true, version = false;
    while (fgets(line, sizeof line, file)) {
        bytes += strlen(line);
        if (bytes > 32768 || (!strchr(line, '\n') && !feof(file))) {
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
        if (key == UI_STRING_COUNT && extra_key(line)) {
            if (candidate->extra_count >= UI_EXTRA_CAP ||
                !valid_text(value, strstr(line, ".help") ? 256 : 96)) {
                valid = false;
                break;
            }
            bool duplicate = false;
            for (unsigned i = 0; i < candidate->extra_count; i++)
                if (!strcmp(candidate->extra[i].key, line))
                    duplicate = true;
            if (duplicate) {
                valid = false;
                break;
            }
            UiTranslation *entry = &candidate->extra[candidate->extra_count++];
            strcpy(entry->key, line);
            strcpy(entry->value, value);
            continue;
        }
        if (key == UI_STRING_COUNT || (key <= UI_FILES && strlen(value) >= 64) || seen[key] ||
            !valid_text(value, 96)) {
            valid = false;
            break;
        }
        seen[key] = true;
        strcpy(candidate->values[key], value);
    }
    valid &= version && !ferror(file);
    fclose(file);
    if (valid) {
        candidate->loaded = true;
        *language = *candidate;
    }
    free(candidate);
}
