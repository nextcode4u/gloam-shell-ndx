/* Native DSi banner sequence playback; no file access after the initial load. */
#include "icons.h"
#include <stdio.h>
#include <string.h>

static uint16_t le16(const unsigned char *p)
{
    return p[0] | p[1] << 8;
}
static uint16_t crc16(const unsigned char *data, size_t bytes)
{
    uint16_t crc = 0xFFFF;
    while (bytes--) {
        crc ^= *data++;
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
    }
    return crc;
}
static void decode_frame(BrowserIcon *icon)
{
    unsigned command = icon->animation_sequence[icon->animation_index];
    const unsigned char *tiles = icon->animation_tiles[(command >> 8) & 7];
    const uint16_t *palette = icon->animation_palettes[(command >> 11) & 7];
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            int source_x = command & 0x4000 ? 31 - x : x;
            int source_y = command & 0x8000 ? 31 - y : y;
            int tile = source_y / 8 * 4 + source_x / 8;
            int byte = tile * 32 + source_y % 8 * 4 + source_x % 8 / 2;
            unsigned color = (tiles[byte] >> ((source_x & 1) * 4)) & 15;
            icon->pixels[y * 32 + x] = color ? 0x8000 | palette[color] : 0;
        }
}
bool icon_load_animation(const char *path, BrowserIcon *icon)
{
    icon->animation_count = 0;
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char header[0x6C], banner_header[10], animation[0x1180];
    bool valid =
        fread(header, 1, sizeof header, file) == sizeof header && !fseek(file, 0, SEEK_END);
    long bytes = valid ? ftell(file) : -1;
    uint32_t offset = valid ? header[0x68] | (uint32_t)header[0x69] << 8 |
                                  (uint32_t)header[0x6A] << 16 | (uint32_t)header[0x6B] << 24
                            : 0;
    valid = valid && bytes >= 0 && offset >= 0x160 && offset <= (uint32_t)bytes &&
            (uint32_t)bytes - offset >= 0x23C0 && !fseek(file, (long)offset, SEEK_SET) &&
            fread(banner_header, 1, sizeof banner_header, file) == sizeof banner_header &&
            le16(banner_header) == 0x103 && !fseek(file, (long)offset + 0x1240, SEEK_SET) &&
            fread(animation, 1, sizeof animation, file) == sizeof animation;
    fclose(file);
    return valid && icon_decode_animation(banner_header, animation, sizeof animation, icon);
}

bool icon_decode_animation(const void *header, const void *data, size_t bytes, BrowserIcon *icon)
{
    icon->animation_count = 0;
    if (!header || !data)
        return false;
    const unsigned char *banner_header = header, *animation = data;
    if (bytes != 0x1180 || le16(banner_header) != 0x103 ||
        crc16(animation, bytes) != le16(banner_header + 8))
        return false;
    int count = 0;
    for (int i = 0; i < 64; i++) {
        unsigned command = le16(animation + 0x1100 + i * 2);
        if (!command)
            break;
        if (!(command & 255))
            return false;
        count++;
    }
    if (!count)
        return false;
    memcpy(icon->animation_tiles, animation, sizeof icon->animation_tiles);
    for (int frame = 0; frame < 8; frame++)
        for (int color = 0; color < 16; color++)
            icon->animation_palettes[frame][color] =
                le16(animation + 0x1000 + frame * 32 + color * 2);
    for (int i = 0; i < count; i++)
        icon->animation_sequence[i] = le16(animation + 0x1100 + i * 2);
    icon->animation_count = count;
    icon->animation_index = 0;
    icon->animation_remaining = icon->animation_sequence[0] & 255;
    decode_frame(icon);
    return true;
}

void icon_animate(BrowserIcon *icon, bool reduced_motion)
{
    if (!icon->animation_count)
        return;
    if (reduced_motion) {
        if (icon->animation_index) {
            icon->animation_index = 0;
            decode_frame(icon);
        }
        icon->animation_remaining = icon->animation_sequence[0] & 255;
        return;
    }
    if (icon->animation_remaining > 1) {
        icon->animation_remaining--;
        return;
    }
    icon->animation_index = (icon->animation_index + 1) % icon->animation_count;
    icon->animation_remaining = icon->animation_sequence[icon->animation_index] & 255;
    decode_frame(icon);
}
