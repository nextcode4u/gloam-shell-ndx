/* Bounded, preconverted startup artwork. See docs/guides/STARTUP_SPLASHES.md. */
#include "splash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned read16(const unsigned char *p)
{
    return p[0] | (unsigned)p[1] << 8;
}
static uint32_t read32(const unsigned char *p)
{
    return read16(p) | (uint32_t)read16(p + 2) << 16;
}
static uint32_t checksum(const unsigned char *data, size_t size)
{
    /* A nibble table keeps validation cheap without a large mutable table. */
    static const uint32_t table[16] = {0,          0x1db71064, 0x3b6e20c8, 0x26d930ac,
                                       0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
                                       0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
                                       0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c};
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        crc = (crc >> 4) ^ table[crc & 15];
        crc = (crc >> 4) ^ table[crc & 15];
    }
    return crc ^ UINT32_MAX;
}
void splash_close(Splash *splash)
{
    free(splash->data);
    memset(splash, 0, sizeof *splash);
}
static bool validate(Splash *splash)
{
    const unsigned char *data = splash->data;
    if (splash->bytes < 64 || memcmp(data, "GLOMSP01", 8) || read32(data + 8) != splash->bytes ||
        read32(data + 12) != checksum(data + 16, splash->bytes - 16))
        return false;
    splash->duration = read16(data + 16);
    splash->poster = read16(data + 18);
    splash->count = data[20];
    if (!splash->duration || splash->duration > SPLASH_TICK_CAP ||
        splash->poster >= splash->duration || !splash->count || splash->count > SPLASH_LAYER_CAP ||
        data[21] || data[22] || data[23])
        return false;
    size_t offset = 32 + 32 * splash->count;
    if (offset > splash->bytes)
        return false;
    for (unsigned i = 0; i < splash->count; i++) {
        const unsigned char *record = data + 32 + 32 * i;
        SplashLayer *layer = &splash->layers[i];
        layer->screen = record[0];
        layer->format = record[1];
        layer->effect = record[2];
        layer->width = read16(record + 4);
        layer->height = read16(record + 6);
        layer->frames = read16(record + 8);
        layer->hold = read16(record + 10);
        layer->x = (int16_t)read16(record + 12);
        layer->y = (int16_t)read16(record + 14);
        layer->delay = read16(record + 16);
        layer->transition = read16(record + 18);
        if (layer->screen > 1 || layer->format > 1 || layer->effect > 4 || record[3] ||
            read32(record + 28) || !layer->width || layer->width > 256 || !layer->height ||
            layer->height > 192 || !layer->frames || layer->frames > 120 || !layer->hold ||
            layer->hold > SPLASH_TICK_CAP || layer->delay >= splash->duration ||
            layer->transition > splash->duration || (layer->effect && !layer->transition) ||
            layer->x < 0 || layer->y < 0 || layer->x + layer->width > 256 ||
            layer->y + layer->height > 192)
            return false;
        size_t bytes = layer->width * layer->height * layer->frames * (layer->format ? 1u : 2u) +
                       (layer->format ? 512u : 0u);
        if (read32(record + 20) != offset || read32(record + 24) != bytes ||
            bytes > splash->bytes - offset)
            return false;
        layer->palette = layer->format ? data + offset : NULL;
        layer->pixels = data + offset + (layer->format ? 512 : 0);
        offset = (offset + bytes + 3) & ~(size_t)3;
        if (offset > splash->bytes)
            return false;
    }
    unsigned sound_bytes = read32(data + 28);
    if (read32(data + 24) != (sound_bytes ? offset : 0) || (sound_bytes & 3) ||
        sound_bytes > 320000 || sound_bytes != splash->bytes - offset)
        return false;
    splash->sound = sound_bytes ? (const int16_t *)(data + offset) : NULL;
    splash->sound_bytes = sound_bytes;
    return true;
}
bool splash_load(Splash *splash, const char *path)
{
    splash_close(splash);
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    bool valid = !fseek(file, 0, SEEK_END);
    long size = valid ? ftell(file) : -1;
    valid = size >= 64 && (unsigned long)size <= SPLASH_FILE_CAP && !fseek(file, 0, SEEK_SET);
    if (valid) {
        splash->bytes = (size_t)size;
        splash->data = malloc(splash->bytes);
        valid = splash->data && fread(splash->data, 1, splash->bytes, file) == splash->bytes;
    }
    fclose(file);
    if (valid && validate(splash))
        return true;
    splash_close(splash);
    return false;
}
bool splash_playing(const Splash *splash, unsigned ticks, bool loading, bool play_once,
                    bool skipped)
{
    return splash->data && !skipped && (loading || (play_once && ticks < splash->duration));
}
static uint16_t blend(uint16_t color, uint16_t behind, unsigned amount)
{
    unsigned inverse = 32 - amount;
    return 0x8000 | (((color & 31) * amount + (behind & 31) * inverse) >> 5) |
           (((((color >> 5) & 31) * amount + ((behind >> 5) & 31) * inverse) >> 5) << 5) |
           (((((color >> 10) & 31) * amount + ((behind >> 10) & 31) * inverse) >> 5) << 10);
}
static unsigned layer_state(const SplashLayer *layer, unsigned ticks)
{
    if (ticks < layer->delay)
        return UINT32_MAX;
    unsigned elapsed = ticks - layer->delay;
    unsigned frame = elapsed / layer->hold;
    if (frame >= layer->frames)
        frame = layer->frames - 1;
    unsigned progress =
        !layer->effect || elapsed >= layer->transition ? 32 : elapsed * 32 / layer->transition;
    return frame * 64 + progress;
}
bool splash_needs_frame(const Splash *splash, unsigned previous, unsigned ticks,
                        bool reduced_motion)
{
    if (!splash->data)
        return false;
    if (previous == UINT32_MAX)
        return true;
    if (reduced_motion)
        return false;
    for (unsigned i = 0; i < splash->count; i++)
        if (layer_state(&splash->layers[i], previous) != layer_state(&splash->layers[i], ticks))
            return true;
    return false;
}
void splash_render(const Splash *splash, unsigned ticks, bool reduced_motion,
                   uint16_t top[256 * 192], uint16_t bottom[256 * 192])
{
    for (unsigned p = 0; p < 256 * 192; p++)
        top[p] = bottom[p] = 0x8000;
    if (!splash->data)
        return;
    if (reduced_motion)
        ticks = splash->poster;
    for (unsigned i = 0; i < splash->count; i++) {
        const SplashLayer *layer = &splash->layers[i];
        if (!reduced_motion && ticks < layer->delay)
            continue;
        unsigned elapsed = ticks > layer->delay ? ticks - layer->delay : 0;
        unsigned frame = elapsed / layer->hold;
        if (frame >= layer->frames)
            frame = layer->frames - 1;
        unsigned progress = reduced_motion || !layer->effect || elapsed >= layer->transition
                                ? 32
                                : elapsed * 32 / layer->transition;
        int x = layer->x, y = layer->y;
        if (layer->effect == 2)
            y += (32 - progress) * 24 / 32;
        if (layer->effect == 3)
            x += (32 - progress) * 24 / 32;
        unsigned visible = layer->effect == 4 ? layer->width * progress / 32 : layer->width;
        uint16_t *destination = layer->screen ? bottom : top;
        for (unsigned row = 0; row < layer->height && y + row < 192; row++)
            for (unsigned col = 0; col < visible && x + col < 256; col++) {
                size_t pixel = (frame * layer->height + row) * layer->width + col;
                uint16_t color = layer->format ? read16(layer->palette + layer->pixels[pixel] * 2)
                                               : read16(layer->pixels + pixel * 2);
                if (!(color & 0x8000))
                    continue;
                uint16_t *out = destination + (y + row) * 256 + x + col;
                *out = layer->effect == 1 && progress < 32 ? blend(color, *out, progress) : color;
            }
    }
}
