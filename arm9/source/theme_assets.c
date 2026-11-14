/* Prepared icons/components replace dozens of opens and PNG decodes. */
#include "theme_assets.h"
#include "image_decode.h"
#include "paths.h"
#include "themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAW_CAP (192 * 1024)
static struct {
    unsigned char *data;
    const unsigned char *icons[THEME_ICON_COUNT], *components[THEME_COMPONENT_COUNT];
    char path[PATH_CAP];
} prepared;
static unsigned read16(const unsigned char *p)
{
    return p[0] | ((unsigned)p[1] << 8);
}
static unsigned read32(const unsigned char *p)
{
    return read16(p) | (read16(p + 2) << 16);
}
void theme_assets_end(void)
{
    free(prepared.data);
    memset(&prepared, 0, sizeof prepared);
}
void theme_assets_begin(const ThemePackage *package)
{
    theme_assets_end();
    if (!*package->prepared_assets)
        return;
    FILE *file = fopen(package->prepared_assets, "rb");
    if (!file)
        return;
    unsigned char header[16];
    bool valid =
        fread(header, 1, sizeof header, file) == sizeof header && !memcmp(header, "GLMART01", 8);
    unsigned raw_size = valid ? read32(header + 8) : 0;
    unsigned count = valid ? read32(header + 12) : 0;
    valid = valid && raw_size > 0 && raw_size <= RAW_CAP && count > 0 &&
            count <= THEME_ICON_COUNT + THEME_COMPONENT_COUNT && !fseek(file, 0, SEEK_END);
    long bytes = valid ? ftell(file) - 16 : -1;
    valid = valid && bytes >= 6 && bytes <= (long)raw_size + 1024 && !fseek(file, 16, SEEK_SET);
    unsigned char *packed = valid ? malloc((size_t)bytes) : NULL;
    prepared.data = valid ? malloc(raw_size) : NULL;
    valid = packed && prepared.data && fread(packed, 1, (size_t)bytes, file) == (size_t)bytes;
    fclose(file);
    if (valid)
        valid = image_decode_zlib(prepared.data, raw_size, packed, (size_t)bytes);
    free(packed);
    unsigned offset = 0;
    for (unsigned entry = 0; valid && entry < count; entry++) {
        if (offset > raw_size || raw_size - offset < 8) {
            valid = false;
            break;
        }
        const unsigned char *record = prepared.data + offset;
        unsigned group = record[0], index = read16(record + 2), size = read32(record + 4);
        offset += 8;
        if (group > 1 || record[1] || size != (group ? 2304u : 2048u) ||
            index >= (group ? THEME_COMPONENT_COUNT : THEME_ICON_COUNT) ||
            size > raw_size - offset) {
            valid = false;
            break;
        }
        const unsigned char **slot = group ? &prepared.components[index] : &prepared.icons[index];
        if (*slot) {
            valid = false;
            break;
        }
        *slot = prepared.data + offset;
        offset += size;
    }
    if (!valid || offset != raw_size)
        theme_assets_end();
    else
        path_copy(prepared.path, package->prepared_assets, sizeof prepared.path);
}
bool theme_assets_copy(const ThemePackage *package, bool component, unsigned index,
                       uint16_t pixels[1024], uint8_t coverage[256])
{
    if (!prepared.data || strcmp(prepared.path, package->prepared_assets) ||
        index >= (component ? THEME_COMPONENT_COUNT : THEME_ICON_COUNT))
        return false;
    const unsigned char *source = component ? prepared.components[index] : prepared.icons[index];
    if (!source)
        return false;
    for (unsigned i = 0; i < 1024; i++) {
        pixels[i] = read16(source + i * 2);
        if (component && !coverage && ((source[2048 + i / 4] >> ((i % 4) * 2)) & 3) < 2)
            pixels[i] = 0;
    }
    if (component && coverage)
        memcpy(coverage, source + 2048, 256);
    return true;
}
