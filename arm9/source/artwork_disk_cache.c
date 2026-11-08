/* A fixed 64-slot cache bounds disk usage without a startup directory scan.
 * Collisions replace disposable thumbnails; full source identity is checked. */
#include "artwork_disk_cache.h"
#include "paths.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define CACHE_SLOTS 64
#define HEADER_SIZE 56
static char cache_directory[PATH_CAP];

static uint32_t checksum(uint32_t hash, const void *data, size_t length)
{
    const unsigned char *bytes = data;
    for (size_t i = 0; i < length; i++)
        hash = (hash ^ bytes[i]) * 16777619U;
    return hash;
}
static void put_value(unsigned char *bytes, uint64_t value, unsigned count)
{
    for (unsigned i = 0; i < count; i++)
        bytes[i] = (unsigned char)(value >> (8 * i));
}
static uint64_t get_value(const unsigned char *bytes, unsigned count)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < count; i++)
        value |= (uint64_t)bytes[i] << (8 * i);
    return value;
}
static bool slot_path(char path[PATH_CAP], unsigned slot, bool temporary)
{
    char name[24];
    snprintf(name, sizeof name, "art-%02x.%s", slot, temporary ? "tmp" : "bin");
    return *cache_directory && path_join(path, cache_directory, name);
}
static unsigned source_slot(const char *source, bool embedded)
{
    return (checksum(2166136261U, source, strlen(source)) ^ (embedded ? 1U : 0U)) % CACHE_SLOTS;
}
void artwork_disk_cache_configure(const char *root, bool enabled)
{
    cache_directory[0] = 0;
    if (enabled && root && !path_join(cache_directory, root, "_nds/dsi-frontend/cache/artwork"))
        cache_directory[0] = 0;
}

bool artwork_disk_cache_read(Artwork *artwork, const char *source, bool embedded,
                             const struct stat *status)
{
    char path[PATH_CAP], stored_source[PATH_CAP];
    if (!slot_path(path, source_slot(source, embedded), false))
        return false;
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char header[HEADER_SIZE] = {0};
    bool valid = fread(header, 1, sizeof header, file) == sizeof header;
    unsigned width = valid ? get_value(header + 32, 2) : 0;
    unsigned height = valid ? get_value(header + 34, 2) : 0;
    size_t length = strlen(source), pixels = width * height;
    valid = valid && !memcmp(header, "DFART001", 8) && width && width <= 96 && height &&
            height <= 104 && get_value(header + 8, 8) == (uint64_t)status->st_size &&
            get_value(header + 16, 8) == (uint64_t)status->st_mtime &&
            get_value(header + 24, 8) == (uint64_t)status->st_ino &&
            get_value(header + 36, 4) == (unsigned)embedded && length < PATH_CAP &&
            get_value(header + 40, 4) == length && get_value(header + 44, 4) == pixels * 2;
    uint32_t expected = valid ? (uint32_t)get_value(header + 48, 4) : 0;
    put_value(header + 48, 0, 4);
    uint32_t hash = checksum(2166136261U, header, sizeof header);
    if (valid) {
        valid = fread(stored_source, 1, length, file) == length &&
                !memcmp(stored_source, source, length);
        hash = checksum(hash, source, length);
    }
    /* Small byte chunks keep the format little-endian on every host. */
    for (size_t offset = 0; valid && offset < pixels;) {
        unsigned char bytes[512];
        size_t count = pixels - offset;
        if (count > sizeof bytes / 2)
            count = sizeof bytes / 2;
        valid = fread(bytes, 2, count, file) == count;
        if (!valid)
            break;
        hash = checksum(hash, bytes, count * 2);
        for (size_t i = 0; i < count; i++)
            artwork->pixels[offset + i] = (uint16_t)get_value(bytes + i * 2, 2);
        offset += count;
    }
    valid = valid && hash == expected && fgetc(file) == EOF && !ferror(file);
    if (fclose(file))
        valid = false;
    if (valid) {
        artwork->width = width;
        artwork->height = height;
        artwork->ready = true;
    }
    return valid;
}

void artwork_disk_cache_write(const Artwork *artwork, const char *source, bool embedded,
                              const struct stat *status)
{
    char path[PATH_CAP], temporary[PATH_CAP];
    unsigned slot = source_slot(source, embedded);
    size_t length = strlen(source);
    if (!artwork->ready || artwork->width < 1 || artwork->width > 96 || artwork->height < 1 ||
        artwork->height > 104 || length >= PATH_CAP || !slot_path(path, slot, false) ||
        !slot_path(temporary, slot, true))
        return;
    /* The existing private cache parent belongs to the frontend. On read-only
     * cards or missing parents this simply leaves the RAM cache operational. */
    if (mkdir(cache_directory, 0777) && errno != EEXIST)
        return;
    unsigned char header[HEADER_SIZE] = {0};
    memcpy(header, "DFART001", 8);
    put_value(header + 8, (uint64_t)status->st_size, 8);
    put_value(header + 16, (uint64_t)status->st_mtime, 8);
    put_value(header + 24, (uint64_t)status->st_ino, 8);
    put_value(header + 32, artwork->width, 2);
    put_value(header + 34, artwork->height, 2);
    put_value(header + 36, embedded, 4);
    put_value(header + 40, length, 4);
    size_t pixels = (size_t)artwork->width * artwork->height;
    put_value(header + 44, pixels * 2, 4);
    uint32_t hash = checksum(2166136261U, header, sizeof header);
    hash = checksum(hash, source, length);
    /* Hash pixels before opening a writer, without another full-size buffer. */
    for (size_t i = 0; i < pixels; i++) {
        unsigned char bytes[2];
        put_value(bytes, artwork->pixels[i], 2);
        hash = checksum(hash, bytes, 2);
    }
    put_value(header + 48, hash, 4);
    FILE *file = fopen(temporary, "wb");
    if (!file)
        return;
    bool valid = fwrite(header, 1, sizeof header, file) == sizeof header &&
                 fwrite(source, 1, length, file) == length;
    for (size_t offset = 0; valid && offset < pixels;) {
        unsigned char bytes[512];
        size_t count = pixels - offset;
        if (count > sizeof bytes / 2)
            count = sizeof bytes / 2;
        for (size_t i = 0; i < count; i++)
            put_value(bytes + i * 2, artwork->pixels[offset + i], 2);
        valid = fwrite(bytes, 2, count, file) == count;
        offset += count;
    }
    if (fclose(file))
        valid = false;
    if (valid) {
        /* Cache loss during replacement is harmless: the source is decoded
         * again. Never claim this provides FAT power-loss atomicity. */
        remove(path);
        if (!rename(temporary, path))
            return;
    }
    remove(temporary);
}

bool artwork_disk_cache_clear(void)
{
    if (!*cache_directory)
        return false;
    bool valid = true;
    for (unsigned slot = 0; slot < CACHE_SLOTS; slot++) {
        for (int temporary = 0; temporary < 2; temporary++) {
            char path[PATH_CAP];
            if (!slot_path(path, slot, temporary))
                return false;
            if (remove(path) && errno != ENOENT)
                valid = false;
        }
    }
    return valid;
}
