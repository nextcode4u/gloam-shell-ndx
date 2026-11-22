/* Prepared icons/components replace dozens of opens and PNG decodes. */
#include "theme_assets.h"
#include "image_decode.h"
#include "image_input.h"
#include "paths.h"
#include "themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define RAW_CAP (512 * 1024)
static struct {
    unsigned char *data;
    const unsigned char *icons[THEME_ICON_COUNT], *components[THEME_COMPONENT_COUNT];
    const unsigned char *backgrounds[2], *materials[2];
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
    bool read = fread(header, 1, sizeof header, file) == sizeof header;
    bool extended = read && !memcmp(header, "GLMART02", 8);
    bool valid = read && (extended || !memcmp(header, "GLMART01", 8));
    unsigned raw_size = valid ? read32(header + 8) : 0;
    unsigned count = valid ? read32(header + 12) : 0;
    size_t total_bytes = 0;
    valid = valid && raw_size > 0 && raw_size <= (extended ? RAW_CAP : 192 * 1024) && count > 0 &&
            count <= THEME_ICON_COUNT + THEME_COMPONENT_COUNT + (extended ? 4u : 0u) &&
            image_file_size(file, sizeof header + 6, sizeof header + raw_size + 1024, &total_bytes);
    size_t bytes = valid ? total_bytes - sizeof header : 0;
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
        if (record[1] || size > raw_size - offset) {
            valid = false;
            break;
        }
        const unsigned char **slot = NULL;
        if (group == 0 && index < THEME_ICON_COUNT && size == 2048)
            slot = &prepared.icons[index];
        else if (group == 1 && index < THEME_COMPONENT_COUNT && size == 2304)
            slot = &prepared.components[index];
        else if (extended && group == 2 && index < 2 && size >= 8 + 256 * 192 * 2) {
            unsigned flags = read32(prepared.data + offset + 4);
            unsigned expected = 8 + 256 * 192 * 2 + ((flags & 4) ? 256 * 94 * 2 : 0);
            if ((flags & ~7u) || !(flags & 1) || ((flags & 4) && (index || !(flags & 2))) ||
                size != expected) {
                valid = false;
                break;
            }
            slot = &prepared.backgrounds[index];
        } else if (extended && group == 3 && index < 2 && size == 4 + 256 * 192 / 2)
            slot = &prepared.materials[index];
        if (!slot) {
            valid = false;
            break;
        }
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

/* Prepared pixels are only valid for the exact source paths/sizes and blend
 * parameters used by the authoring converter. Personal overrides fall back. */
static uint32_t hash_bytes(uint32_t hash, const void *data, size_t bytes)
{
    const unsigned char *p = data;
    for (size_t i = 0; i < bytes; i++)
        hash = (hash ^ p[i]) * 16777619u;
    return hash;
}
static uint32_t hash_number(uint32_t hash, uint32_t number)
{
    unsigned char bytes[4] = {number, number >> 8, number >> 16, number >> 24};
    return hash_bytes(hash, bytes, sizeof bytes);
}
static bool hash_source(uint32_t *hash, const ThemePackage *package, const char *path)
{
    if (!*path) {
        *hash = hash_bytes(*hash, "", 1);
        *hash = hash_number(*hash, 0);
        return true;
    }
    size_t length = strlen(package->directory);
    if (!length || strncmp(path, package->directory, length) || path[length] != '/')
        return false;
    struct stat status;
    if (stat(path, &status) || !S_ISREG(status.st_mode) || status.st_size < 0 ||
        status.st_size > 1024 * 1024)
        return false;
    const char *relative = path + length + 1;
    *hash = hash_bytes(*hash, relative, strlen(relative) + 1);
    *hash = hash_number(*hash, (uint32_t)status.st_size);
    return true;
}
bool theme_assets_background(const ThemePackage *package, unsigned screen,
                             uint16_t pixels[256 * 192], uint16_t **scene, bool *wallpaper_ready)
{
    if (screen > 1 || !prepared.data || strcmp(prepared.path, package->prepared_assets))
        return false;
    const unsigned char *data = prepared.backgrounds[screen];
    if (!data)
        return false;
    uint32_t key = hash_number(2166136261u, package->palette.bg);
    key = hash_number(key, package->background_strength);
    key = hash_number(key, themes_content_opacity(package, screen));
    key = hash_number(key, screen == 0 && package->top_layout == 3);
    if (!hash_source(&key, package, package->backgrounds[screen]) ||
        !hash_source(&key, package, package->reading_surfaces[screen]) || key != read32(data))
        return false;
    unsigned flags = read32(data + 4);
    *scene = NULL;
    if (flags & 4) {
        *scene = malloc(256 * 94 * sizeof **scene);
        if (*scene)
            memcpy(*scene, data + 8 + 256 * 192 * 2, 256 * 94 * sizeof **scene);
    }
    memcpy(pixels, data + 8, 256 * 192 * sizeof *pixels);
    *wallpaper_ready = (flags & 2) != 0;
    return true;
}
bool theme_assets_material(const ThemePackage *package, unsigned index,
                           uint8_t shades[256 * 192 / 2])
{
    if (index > 1 || !prepared.data || strcmp(prepared.path, package->prepared_assets))
        return false;
    const unsigned char *data = prepared.materials[index];
    uint32_t key = 2166136261u;
    if (!data || !hash_source(&key, package, package->component_material[index]) ||
        key != read32(data))
        return false;
    memcpy(shades, data + 4, 256 * 192 / 2);
    return true;
}
