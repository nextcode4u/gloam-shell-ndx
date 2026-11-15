/* Optional nine-slice artwork. Prepare once on theme changes; drawing does no I/O. */
#include "app.h"
#include "icons.h"
#include "image_decode.h"
#include "paths.h"
#include "theme_assets.h"
#include "themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char directory[PATH_CAP];
    uint16_t *images[THEME_COMPONENT_COUNT];
    uint8_t *coverage[THEME_COMPONENT_COUNT];
    char material_paths[2][PATH_CAP];
    uint8_t *materials[2];
    uint16_t material_colors[THEME_ROW + 1][16];
    bool use_coverage, ready;
} ComponentBank;
static ComponentBank banks[2];
static const char *const names[THEME_COMPONENT_COUNT] = {"header",
                                                         "footer",
                                                         "panel",
                                                         "row-selected",
                                                         "tab-selected",
                                                         "card",
                                                         "card-selected",
                                                         "row",
                                                         "icon-frame",
                                                         "artwork-frame",
                                                         "battery-empty",
                                                         "battery-low",
                                                         "battery-half",
                                                         "battery-high",
                                                         "battery-full",
                                                         "battery-charging",
                                                         "battery-unknown",
                                                         "wireless-idle",
                                                         "update-available",
                                                         "check-off",
                                                         "check-on",
                                                         "toggle-off",
                                                         "toggle-on",
                                                         "arrow-left",
                                                         "arrow-right",
                                                         "arrow-up",
                                                         "arrow-down",
                                                         "scroll-track",
                                                         "scroll-thumb",
                                                         "busy"};

static void clear_bank(ComponentBank *bank)
{
    for (unsigned i = 0; i < THEME_COMPONENT_COUNT; i++) {
        free(bank->images[i]);
        free(bank->coverage[i]);
    }
    free(bank->materials[0]);
    free(bank->materials[1]);
    memset(bank, 0, sizeof *bank);
}

static uint8_t *load_material(const char *path)
{
    if (!*path)
        return NULL;
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;
    bool valid = !fseek(file, 0, SEEK_END);
    long bytes = valid ? ftell(file) : -1;
    unsigned char *encoded = bytes > 0 && bytes <= 256 * 1024 ? malloc((size_t)bytes) : NULL;
    uint8_t *field = encoded ? malloc(256 * 192 / 2) : NULL;
    valid = field && !fseek(file, 0, SEEK_SET) &&
            fread(encoded, 1, (size_t)bytes, file) == (size_t)bytes;
    fclose(file);
    valid = valid && image_decode_material(field, encoded, (size_t)bytes);
    free(encoded);
    if (!valid) {
        free(field);
        return NULL;
    }
    return field;
}

static bool bank_matches(const ComponentBank *bank, const ThemePackage *package)
{
    return bank->ready && package->component_coverage == bank->use_coverage &&
           !strcmp(package->component_directory, bank->directory) &&
           !strcmp(package->component_material[0], bank->material_paths[0]) &&
           !strcmp(package->component_material[1], bank->material_paths[1]);
}

static void prepare_materials(ComponentBank *bank, const ThemePackage *package)
{
    if (!*package->component_material[0] && !*package->component_material[1])
        return;
    for (unsigned i = 0; i < 2; i++) {
        path_copy(bank->material_paths[i], package->component_material[i], PATH_CAP);
        if (!i || strcmp(bank->material_paths[0], bank->material_paths[1]))
            bank->materials[i] = load_material(bank->material_paths[i]);
        app_loading_checkpoint();
    }
    for (unsigned component = 0; component <= THEME_ROW; component++) {
        const uint16_t *image = bank->images[component];
        unsigned base = image ? image[16 * 32 + 16] : 0;
        for (int shade = 0; shade < 16; shade++) {
            uint16_t color = 0x8000;
            for (unsigned shift = 0; shift < 15; shift += 5) {
                int channel = (int)((base >> shift) & 31) + shade - 8;
                channel = channel < 0 ? 0 : channel > 31 ? 31 : channel;
                color |= (unsigned)channel << shift;
            }
            bank->material_colors[component][shade] = color;
        }
    }
}
void themes_components_load(const ThemePackage *package, bool preview)
{
    ComponentBank *bank = &banks[preview ? 1 : 0];
    clear_bank(bank);
    if (!*package->component_directory)
        return;
    path_copy(bank->directory, package->component_directory, PATH_CAP);
    bank->use_coverage = package->component_coverage;
    unsigned count = package->control_assets ? THEME_COMPONENT_COUNT : THEME_CHECK_OFF;
    for (unsigned i = 0; i < count; i++) {
        char name[40], path[PATH_CAP];
        snprintf(name, sizeof name, "%s.png", names[i]);
        if (!path_join(path, bank->directory, name))
            continue;
        uint16_t *image = malloc(32 * 32 * sizeof *image);
        uint8_t coverage[256];
        if (image &&
            (theme_assets_copy(package, true, i, image, bank->use_coverage ? coverage : NULL) ||
             icon_load_png_coverage(path, image, bank->use_coverage ? coverage : NULL))) {
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
    prepare_materials(bank, package);
    bank->ready = true;
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
        if (bank_matches(&banks[i], package))
            return banks[i].images[component];
    return NULL;
}

const uint8_t *themes_component_alpha(const ThemePackage *package, ThemeComponent component)
{
    if ((unsigned)component >= THEME_COMPONENT_COUNT || !package->component_coverage ||
        !*package->component_directory)
        return NULL;
    for (int i = 1; i >= 0; i--)
        if (bank_matches(&banks[i], package))
            return banks[i].coverage[component];
    return NULL;
}

const uint8_t *themes_component_material(const ThemePackage *package, ThemeComponent component,
                                         const uint16_t **palette)
{
    *palette = NULL;
    if ((unsigned)component > THEME_ROW || !*package->component_directory ||
        (!*package->component_material[0] && !*package->component_material[1]))
        return NULL;
    unsigned selected = component == THEME_ROW_SELECTED || component == THEME_CARD_SELECTED;
    for (int i = 1; i >= 0; i--) {
        const ComponentBank *bank = &banks[i];
        if (bank_matches(bank, package)) {
            if (!bank->images[component] || !(bank->images[component][16 * 32 + 16] & 0x8000))
                return NULL;
            const uint8_t *field = bank->materials[selected];
            if (!field)
                field = bank->materials[0];
            if (field)
                *palette = bank->material_colors[component];
            return field;
        }
    }
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
