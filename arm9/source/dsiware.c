/* DSiWare uses independent public/private FAT12 volumes, not a raw DS .sav. */
#include "dsiware.h"
#include "paths.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifndef HOST_PREVIEW
#include <fat.h>
#endif

static uint32_t le32(const unsigned char *data)
{
    return data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}
static void put16(unsigned char *out, unsigned value)
{
    out[0] = value;
    out[1] = value >> 8;
}
static void put32(unsigned char *out, uint32_t value)
{
    for (int i = 0; i < 4; i++)
        out[i] = value >> (i * 8);
}
static bool valid_size(uint32_t bytes)
{
    return bytes >= 4096 && bytes <= 32 * 1024 * 1024 && !(bytes & (bytes - 1));
}
static bool choose_save(const char *target, const char *extension, int slot, uint32_t bytes,
                        char output[PATH_CAP], char error[160])
{
    output[0] = 0;
    if (!bytes)
        return true;
    if (!valid_size(bytes)) {
        snprintf(error, 160,
                 "Unsupported DSiWare %s save size. New FAT12 volumes require 4 KiB-32 MiB, "
                 "power-of-two sizes.",
                 extension);
        return false;
    }
    char folder[PATH_CAP], stem[256], name[PATH_CAP];
    path_copy(folder, target, sizeof folder);
    char *slash = strrchr(folder, '/');
    if (!slash)
        return false;
    path_copy(stem, slash + 1, sizeof stem);
    slash[1] = 0;
    char *dot = strrchr(stem, '.');
    if (dot)
        *dot = 0;
    unsigned found = 0;
    for (int variant = 0; !slot && variant < 3; variant++) {
        int length = snprintf(name, sizeof name, variant == 2 ? "saves/%s.%s" : "%s.%s",
                              variant == 1 ? strrchr(target, '/') + 1 : stem, extension);
        char path[PATH_CAP];
        if (length >= (int)sizeof name || !path_join(path, folder, name))
            return false;
        struct stat status;
        if (stat(path, &status)) {
            if (errno != ENOENT)
                return false;
            continue;
        }
        if (!S_ISREG(status.st_mode) || (uint64_t)status.st_size != bytes) {
            snprintf(error, 160,
                     "Existing DSiWare %s save has a conflicting type/size. It was left unchanged.",
                     extension);
            return false;
        }
        path_copy(output, path, PATH_CAP);
        found++;
    }
    if (found > 1) {
        snprintf(error, 160,
                 "Multiple DSiWare %s saves exist. Keep the intended slot-0 save at one recognized "
                 "path; no files moved.",
                 extension);
        return false;
    }
    if (!found) {
        int length = slot ? snprintf(name, sizeof name, "saves/%s.%s%d", stem, extension, slot)
                          : snprintf(name, sizeof name, "saves/%s.%s", stem, extension);
        if (length >= (int)sizeof name || !path_join(output, folder, name))
            return false;
        struct stat status;
        if (!stat(output, &status) &&
            (!S_ISREG(status.st_mode) || (uint64_t)status.st_size != bytes)) {
            snprintf(error, 160,
                     "Existing DSiWare save slot has a conflicting size. No resize was attempted.");
            return false;
        }
    }
    return true;
}
bool dsiware_prepare(const App *app, const char *target, const unsigned char header[0x240],
                     LaunchPlan *plan, char error[160])
{
    if (!app->dsi_mode) {
        snprintf(error, 160, "This DSiWare route requires DSi mode and DSi SD access.");
        return false;
    }
    const GameOptions *options = &plan->options;
    if (options->values[GAME_MODE] == 0 || options->values[GAME_CPU] == 0 ||
        options->values[GAME_VRAM] == 0 || options->values[GAME_SAVE_SIZE] >= 0) {
        snprintf(error, 160,
                 "DSiWare needs DSi mode, CPU/VRAM boosts and Auto save size. Set these to Auto/On "
                 "in Title options.");
        return false;
    }
    uint32_t rom_bytes = le32(header + 0x210);
    if (rom_bytes && rom_bytes < 0x240) {
        snprintf(error, 160, "DSiWare extended ROM size is invalid.");
        return false;
    }
    plan->route = LAUNCH_DSIWARE;
    plan->save_bytes = le32(header + 0x238);
    plan->private_bytes = le32(header + 0x23C);
    plan->options.values[GAME_MODE] = 1;
    plan->options.values[GAME_CPU] = plan->options.values[GAME_VRAM] = 1;
    return choose_save(target, "pub", options->values[GAME_SLOT], plan->save_bytes, plan->save,
                       error) &&
           choose_save(target, "prv", options->values[GAME_SLOT], plan->private_bytes,
                       plan->private_save, error);
}
bool dsiware_create_save(const char *path, uint32_t bytes, char error[160])
{
    if (!bytes)
        return !*path;
    if (!valid_size(bytes) || !*path)
        return false;
    struct stat status;
    if (!stat(path, &status)) {
        bool valid = S_ISREG(status.st_mode) && (uint64_t)status.st_size == bytes;
        if (!valid)
            snprintf(error, 160, "Existing DSiWare save size conflicts; it was left unchanged.");
        return valid;
    }
    if (errno != ENOENT)
        return false;
    unsigned sectors = bytes / 512, roots = bytes < 0x8C000 ? 32 : 512;
    unsigned root_sectors = roots / 16, clusters = 0, fat_sectors = 0, cluster_sectors = 1;
    for (; cluster_sectors <= 64; cluster_sectors *= 2) {
        unsigned fat = 1;
        for (int attempt = 0; attempt < 16; attempt++) {
            if (sectors <= 1 + 2 * fat + root_sectors)
                break;
            clusters = (sectors - 1 - 2 * fat - root_sectors) / cluster_sectors;
            unsigned needed = (((clusters + 2) * 3 + 1) / 2 + 511) / 512;
            if (needed == fat) {
                fat_sectors = fat;
                break;
            }
            fat = needed;
        }
        if (fat_sectors && clusters && clusters < 4085)
            break;
        fat_sectors = 0;
    }
    if (!fat_sectors || cluster_sectors > 64) {
        snprintf(error, 160, "Save geometry cannot be represented as FAT12.");
        return false;
    }
    char directory[PATH_CAP], temporary[PATH_CAP];
    path_copy(directory, path, sizeof directory);
    char *slash = strrchr(directory, '/');
    if (!slash ||
        snprintf(temporary, sizeof temporary, "%s.frontend-new", path) >= (int)sizeof temporary)
        return false;
    *slash = 0;
    if (mkdir(directory, 0777) &&
        (errno != EEXIST || stat(directory, &status) || !S_ISDIR(status.st_mode)))
        return false;
    FILE *file = fopen(temporary, "wbx");
    if (!file) {
        snprintf(
            error, 160,
            "Cannot create DSiWare save staging file. Check SD space or an interrupted launch.");
        return false;
    }
    unsigned char sector[512] = {0};
    sector[0] = 0xEB;
    sector[1] = 0x3C;
    sector[2] = 0x90;
    memcpy(sector + 3, "DSIFRONT", 8);
    put16(sector + 11, 512);
    sector[13] = cluster_sectors;
    put16(sector + 14, 1);
    sector[16] = 2;
    put16(sector + 17, roots);
    put16(sector + 19, sectors < 65536 ? sectors : 0);
    sector[21] = 0xF8;
    put16(sector + 22, fat_sectors);
    put16(sector + 24, 32);
    put16(sector + 26, 64);
    if (sectors >= 65536)
        put32(sector + 32, sectors);
    sector[36] = 0x80;
    sector[38] = 0x29;
    put32(sector + 39, 0x44534946);
    memcpy(sector + 43, "NO NAME    FAT12   ", 19);
    sector[510] = 0x55;
    sector[511] = 0xAA;
    bool valid = fwrite(sector, 1, sizeof sector, file) == sizeof sector;
    memset(sector, 0, sizeof sector);
    for (unsigned index = 1; valid && index < sectors; index++) {
        if (index == 1 || index == 1 + fat_sectors) {
            sector[0] = 0xF8;
            sector[1] = sector[2] = 0xFF;
        }
        valid = fwrite(sector, 1, sizeof sector, file) == sizeof sector;
        sector[0] = sector[1] = sector[2] = 0;
    }
    if (fclose(file))
        valid = false;
    /* A failed access check must not become permission to replace a save. */
    if (valid)
        valid = stat(path, &status) != 0 && errno == ENOENT;
    if (valid)
        valid = !rename(temporary, path);
    if (!valid) {
        remove(temporary);
        snprintf(error, 160, "DSiWare save creation failed. Existing saves were not replaced.");
    }
    return valid;
}
static bool short_name(const char *path, char name[13])
{
#ifndef HOST_PREVIEW
    return FAT_getShortNameFor(path, name);
#else
    /* Host fixtures use actual 8.3 names; never invent FAT aliases on POSIX. */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    size_t length = strlen(base);
    const char *dot = strrchr(base, '.');
    size_t stem = dot ? (size_t)(dot - base) : length;
    if (!stem || stem > 8 || (dot && strlen(dot + 1) > 3) || length > 12)
        return false;
    for (size_t i = 0; i < length; i++) {
        if (!isalnum((unsigned char)base[i]) && base[i] != '.' && base[i] != '_' && base[i] != '-')
            return false;
        name[i] = (char)toupper((unsigned char)base[i]);
    }
    name[length] = 0;
    return true;
#endif
}
bool dsiware_alias(const char *root, const char *path, char output[64])
{
    output[0] = 0;
    if (!*path)
        return true;
    if (!path_is_within_root(path, root))
        return false;
    const char *relative = path + strlen(root);
    while (*relative == '/')
        relative++;
    char current[PATH_CAP];
    path_copy(current, root, sizeof current);
    path_copy(output, "sd:/", 64);
    while (*relative) {
        const char *end = strchr(relative, '/');
        size_t length = end ? (size_t)(end - relative) : strlen(relative);
        char component[256], joined[PATH_CAP], alias[13];
        if (!length || length >= sizeof component)
            return false;
        memcpy(component, relative, length);
        component[length] = 0;
        if (!strcmp(component, ".") || !strcmp(component, "..") ||
            !path_join(joined, current, component) || !short_name(joined, alias))
            return false;
        struct stat status;
        if (stat(joined, &status) || (end && !S_ISDIR(status.st_mode)))
            return false;
        path_copy(current, joined, sizeof current);
        size_t used = strlen(output), extra = strlen(alias);
        if (used + extra + (end ? 1 : 0) >= 62)
            return false;
        memcpy(output + used, alias, extra + 1);
        if (end) {
            output[used + extra] = '/';
            output[used + extra + 1] = 0;
        }
        relative += length;
        if (end)
            relative++;
    }
    return true;
}
