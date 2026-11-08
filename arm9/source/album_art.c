/* Bounded ID3v2.3/v2.4 APIC extraction. Audio payloads are never read in full. */
#include "image_decode.h"
#include "image_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool syncsafe(const unsigned char *p, uint32_t *value)
{
    if ((p[0] | p[1] | p[2] | p[3]) & 0x80)
        return false;
    *value = ((uint32_t)p[0] << 21) | ((uint32_t)p[1] << 14) | ((uint32_t)p[2] << 7) | p[3];
    return true;
}
static uint32_t big32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static bool picture(const unsigned char *data, size_t size, size_t *offset, unsigned *type)
{
    if (size < 5 || data[0] > 3)
        return false;
    unsigned encoding = data[0];
    size_t i = 1;
    while (i < size && data[i])
        i++;
    if (i == size || i + 2 >= size)
        return false;
    size_t mime_size = i - 1;
    if (!((mime_size == 10 && !memcmp(data + 1, "image/jpeg", 10)) ||
          (mime_size == 9 && !memcmp(data + 1, "image/png", 9))))
        return false;
    *type = data[++i];
    i++;
    if (encoding == 1 || encoding == 2) {
        while (i + 1 < size && (data[i] || data[i + 1]))
            i += 2;
        if (i + 1 >= size)
            return false;
        i += 2;
    } else {
        while (i < size && data[i])
            i++;
        if (i == size)
            return false;
        i++;
    }
    if (i >= size || size - i > 1024 * 1024)
        return false;
    *offset = i;
    return true;
}
bool embedded_album_art(Artwork *artwork, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char header[10];
    uint32_t size = 0;
    bool valid = fread(header, 1, 10, file) == 10 && !memcmp(header, "ID3", 3) &&
                 (header[3] == 3 || header[3] == 4) && !(header[5] & 0x80) &&
                 syncsafe(header + 6, &size) && size >= 10 && size <= 1024 * 1024 + 65536;
    unsigned char *tag = valid ? malloc(size) : NULL;
    valid = valid && tag && image_read_exact(file, tag, size);
    fclose(file);
    if (!valid) {
        free(tag);
        return false;
    }
    size_t start = 0;
    if (header[5] & 0x40) {
        uint32_t extended = size >= 4 ? big32(tag) : UINT32_MAX;
        if (header[3] == 3)
            extended = extended >= 6 && extended <= size - 4 ? extended + 4 : UINT32_MAX;
        if (header[3] == 4 && (!syncsafe(tag, &extended) || extended < 6))
            extended = UINT32_MAX;
        start = extended;
    }
    size_t candidate = 0, candidate_size = 0;
    unsigned chosen_type = 0;
    int frames = 0;
    while (start <= size && size - start >= 10 && ++frames <= 256) {
        const unsigned char *frame = tag + start;
        if (!frame[0])
            break;
        uint32_t length = big32(frame + 4);
        if (header[3] == 4 && !syncsafe(frame + 4, &length))
            break;
        if (!length || length > size - start - 10)
            break;
        unsigned unsupported = header[3] == 3 ? frame[9] & 0xE0 : frame[9] & 0x4F;
        if (!memcmp(frame, "APIC", 4) && !unsupported) {
            size_t offset;
            unsigned type;
            if (picture(frame + 10, length, &offset, &type) &&
                (!candidate_size || (type == 3 && chosen_type != 3))) {
                candidate = start + 10 + offset;
                candidate_size = length - offset;
                chosen_type = type;
            }
        }
        start += 10 + length;
    }
    bool decoded = candidate_size && image_decode(artwork, tag + candidate, candidate_size);
    free(tag);
    return decoded;
}
