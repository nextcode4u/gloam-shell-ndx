/* Optional nine-slice artwork. Prepare once on theme changes; drawing does no I/O. */
#include "app.h"
#include "icons.h"
#include "paths.h"
#include "themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char directory[PATH_CAP];
    uint16_t *images[THEME_COMPONENT_COUNT];
    uint8_t *coverage[THEME_COMPONENT_COUNT];
    bool use_coverage;
} ComponentBank;
static ComponentBank banks[2];
static const char *const names[THEME_COMPONENT_COUNT] = {
    "header",          "footer",        "panel",           "row-selected",
    "tab-selected",    "card",          "card-selected",   "row",
    "icon-frame",      "artwork-frame", "battery-empty",   "battery-low",
    "battery-half",    "battery-high",  "battery-full",    "battery-charging",
    "battery-unknown", "wireless-idle", "update-available"};

static void clear_bank(ComponentBank *bank)
{
    for (unsigned i = 0; i < THEME_COMPONENT_COUNT; i++) {
        free(bank->images[i]);
        free(bank->coverage[i]);
    }
    memset(bank, 0, sizeof *bank);
}
void themes_components_load(const ThemePackage *package, bool preview)
{
    ComponentBank *bank = &banks[preview ? 1 : 0];
    clear_bank(bank);
    if (!*package->component_directory)
        return;
    path_copy(bank->directory, package->component_directory, PATH_CAP);
    bank->use_coverage = package->component_coverage;
    for (unsigned i = 0; i < THEME_COMPONENT_COUNT; i++) {
        char name[40], path[PATH_CAP];
        snprintf(name, sizeof name, "%s.png", names[i]);
        if (!path_join(path, bank->directory, name))
            continue;
        uint16_t *image = malloc(32 * 32 * sizeof *image);
        uint8_t coverage[256];
        if (image && icon_load_png_coverage(path, image, bank->use_coverage ? coverage : NULL)) {
            bool partial = false;
            for (unsigned pixel = 0; bank->use_coverage && pixel < 1024; pixel++) {
                unsigned alpha = (coverage[pixel / 4] >> ((pixel % 4) * 2)) & 3;
                partial |= alpha == 1 || alpha == 2;
            }
            if (partial) {
                bank->coverage[i] = malloc(sizeof coverage);
                if (!bank->coverage[i]) {
                    free(image);
                    continue;
                }
                memcpy(bank->coverage[i], coverage, sizeof coverage);
            }
            bank->images[i] = image;
        } else
            free(image);
        app_loading_checkpoint();
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
        if (package->component_coverage == banks[i].use_coverage &&
            !strcmp(package->component_directory, banks[i].directory))
            return banks[i].images[component];
    return NULL;
}

const uint8_t *themes_component_alpha(const ThemePackage *package, ThemeComponent component)
{
    if ((unsigned)component >= THEME_COMPONENT_COUNT || !package->component_coverage ||
        !*package->component_directory)
        return NULL;
    for (int i = 1; i >= 0; i--)
        if (banks[i].use_coverage && !strcmp(package->component_directory, banks[i].directory))
            return banks[i].coverage[component];
    return NULL;
}

ThemeComponent themes_battery_component(int level, bool charging)
{
    if (charging)
        return THEME_BATTERY_CHARGING;
    if (level < 0 || level > 15)
        return THEME_BATTERY_UNKNOWN;
    if (level == 0)
        return THEME_BATTERY_EMPTY;
    if (level <= 3)
        return THEME_BATTERY_LOW;
    if (level <= 7)
        return THEME_BATTERY_HALF;
    if (level <= 11)
        return THEME_BATTERY_HIGH;
    return THEME_BATTERY_FULL;
}
