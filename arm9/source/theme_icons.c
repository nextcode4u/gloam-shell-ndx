/* Optional PNG symbols are decoded during theme changes, never while drawing.
 * Two banks preserve the applied pack while previewing another. Missing or bad
 * images retain the built-in symbol independently of the rest of the pack. */
#include "app.h"
#include "icons.h"
#include "paths.h"
#include "themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char directory[PATH_CAP];
    uint16_t *images[THEME_ICON_COUNT];
} IconBank;
static IconBank banks[2];
static const char *const names[THEME_ICON_COUNT] = {
    "file",      "folder",        "game",        "application", "audio",     "video",
    "book",      "text",          "image",       "save",        "config",    "archive",
    "shortcut",  "system",        "missing",     "settings",    "camera",    "sound",
    "pictochat", "download-play", "system-menu", "slot1",       "tab-home",  "tab-games",
    "tab-apps",  "tab-media",     "tab-system",  "tab-files",   "tab-folder"};

static void clear_bank(IconBank *bank)
{
    for (int i = 0; i < THEME_ICON_COUNT; i++)
        free(bank->images[i]);
    memset(bank, 0, sizeof *bank);
}
void themes_icons_load(const ThemePackage *package, bool preview)
{
    IconBank *bank = &banks[preview ? 1 : 0];
    clear_bank(bank);
    if (!*package->icon_directory)
        return;
    path_copy(bank->directory, package->icon_directory, PATH_CAP);
    for (int i = 0; i < THEME_ICON_COUNT; i++) {
        char filename[40], path[PATH_CAP];
        snprintf(filename, sizeof filename, "%s.png", names[i]);
        if (!path_join(path, bank->directory, filename))
            continue;
        uint16_t *image = malloc(32 * 32 * sizeof *image);
        if (image && icon_load_png(path, image))
            bank->images[i] = image;
        else
            free(image);
        app_loading_checkpoint();
    }
}
void themes_icons_accept(void)
{
    clear_bank(&banks[0]);
    banks[0] = banks[1];
    memset(&banks[1], 0, sizeof banks[1]);
}
void themes_icons_cancel(void)
{
    clear_bank(&banks[1]);
}
const uint16_t *themes_icon(const ThemePackage *package, int index)
{
    if ((unsigned)index >= THEME_ICON_COUNT || !*package->icon_directory)
        return NULL;
    /* Preview wins when previewing the same directory after an on-disk edit. */
    for (int i = 1; i >= 0; i--)
        if (!strcmp(banks[i].directory, package->icon_directory))
            return banks[i].images[index];
    return NULL;
}
