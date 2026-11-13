/* Optional nine-slice artwork. Prepare once on theme changes; drawing does no I/O. */
#include "icons.h"
#include "paths.h"
#include "themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char directory[PATH_CAP];
    uint16_t *images[THEME_COMPONENT_COUNT];
} ComponentBank;
static ComponentBank banks[2];
static const char *const names[THEME_COMPONENT_COUNT] = {
    "header", "footer", "panel", "row-selected", "tab-selected", "card", "card-selected"};

static void clear_bank(ComponentBank *bank)
{
    for (unsigned i = 0; i < THEME_COMPONENT_COUNT; i++)
        free(bank->images[i]);
    memset(bank, 0, sizeof *bank);
}
void themes_components_load(const ThemePackage *package, bool preview)
{
    ComponentBank *bank = &banks[preview ? 1 : 0];
    clear_bank(bank);
    if (!*package->component_directory)
        return;
    path_copy(bank->directory, package->component_directory, PATH_CAP);
    for (unsigned i = 0; i < THEME_COMPONENT_COUNT; i++) {
        char name[40], path[PATH_CAP];
        snprintf(name, sizeof name, "%s.png", names[i]);
        if (!path_join(path, bank->directory, name))
            continue;
        uint16_t *image = malloc(32 * 32 * sizeof *image);
        if (image && icon_load_png(path, image))
            bank->images[i] = image;
        else
            free(image);
    }
}
void themes_components_cancel(void)
{
    clear_bank(&banks[1]);
}
void themes_components_accept(void)
{
    clear_bank(&banks[0]);
    banks[0] = banks[1];
    memset(&banks[1], 0, sizeof banks[1]);
}
const uint16_t *themes_component(const ThemePackage *package, ThemeComponent component)
{
    if ((unsigned)component >= THEME_COMPONENT_COUNT || !*package->component_directory)
        return NULL;
    for (int i = 1; i >= 0; i--)
        if (!strcmp(package->component_directory, banks[i].directory))
            return banks[i].images[component];
    return NULL;
}
