#ifndef GLOAM_UI_STRINGS_H
#define GLOAM_UI_STRINGS_H
#include <stdbool.h>
typedef enum {
    UI_HOME,
    UI_GAMES,
    UI_APPS,
    UI_MEDIA,
    UI_SYSTEM,
    UI_FILES,
    UI_SETTINGS,
    UI_READING_FOLDER,
    UI_BROWSER_HINT,
    UI_TAB_HINT,
    UI_INSTALLED_TITLES,
    UI_EMPTY_FOLDER,
    UI_STRING_COUNT
} UiString;
#define UI_EXTRA_CAP 192
typedef struct {
    char key[48], value[256];
} UiTranslation;
typedef struct {
    UiTranslation extra[UI_EXTRA_CAP];
    unsigned extra_count;
    char values[UI_STRING_COUNT][96];
    bool loaded;
} UiLanguage;
/* Optional partial catalog; absent keys always retain English. No renderer I/O. */
void ui_language_load(UiLanguage *language, const char *root, bool enabled);
const char *ui_translate(const UiLanguage *language, const char *key, const char *fallback);
const char *ui_text(const UiLanguage *language, UiString key);
#endif
