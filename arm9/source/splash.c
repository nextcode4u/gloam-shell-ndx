/* Bounded, preconverted startup artwork. See docs/guides/STARTUP_SPLASHES.md. */
#include "splash.h"
#include "image_decode.h"
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
    for (unsigned i = 0; i < SPLASH_LAYER_CAP; i++)
        free(splash->layers[i].decoded);
    free(splash->data);
    memset(splash, 0, sizeof *splash);
}
static bool validate(Splash *splash)
{
    const unsigned char *data = splash->data;
    bool compressed = splash->bytes >= 8 && !memcmp(data, "GLOMSP03", 8);
    bool motion = compressed || (splash->bytes >= 8 && !memcmp(data, "GLOMSP02", 8));
    if (splash->bytes < 64 || (!motion && memcmp(data, "GLOMSP01", 8)) ||
        read32(data + 8) != splash->bytes ||
        read32(data + 12) != checksum(data + 16, splash->bytes - 16))
        return false;
    splash->duration = read16(data + 16);
    splash->poster = read16(data + 18);
    splash->count = data[20];
    splash->background = motion ? read16(data + 22) : 0x8000;
    if (!splash->duration || splash->duration > SPLASH_TICK_CAP ||
        splash->poster >= splash->duration || !splash->count || splash->count > SPLASH_LAYER_CAP ||
        data[21] || (motion ? !(splash->background & 0x8000) : data[22] || data[23]))
        return false;
    unsigned record_size = motion ? 64 : 32;
    size_t offset = 32 + record_size * splash->count;
    if (offset > splash->bytes)
        return false;
    for (unsigned i = 0; i < splash->count; i++) {
        const unsigned char *record = data + 32 + record_size * i;
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
        layer->motion = motion;
        if (motion) {
            layer->start_x = (int16_t)read16(record + 32);
            layer->start_y = (int16_t)read16(record + 34);
            layer->end_x = (int16_t)read16(record + 36);
            layer->end_y = (int16_t)read16(record + 38);
            layer->start_scale = read16(record + 40);
            layer->end_scale = read16(record + 42);
            layer->start_opacity = record[44];
            layer->end_opacity = record[45];
            layer->easing = record[46];
            layer->repeat = record[47];
            layer->sequence = record[48];
            if (abs(layer->start_x) > 512 || abs(layer->start_y) > 512 || abs(layer->end_x) > 512 ||
                abs(layer->end_y) > 512 || layer->start_scale > 400 || layer->end_scale > 400 ||
                layer->start_opacity > 32 || layer->end_opacity > 32 || layer->easing > 3 ||
                layer->repeat > 2 || layer->sequence > 2 || (layer->repeat && !layer->transition) ||
                abs(layer->x) > 512 || abs(layer->y) > 512)
                return false;
            if (compressed && record[49] > 1)
                return false;
            for (unsigned reserved = compressed ? 50 : 49; reserved < 64; reserved++)
                if (record[reserved])
                    return false;
        }
        if (layer->screen > 1 || layer->format > 1 || layer->effect > 4 || record[3] ||
            read32(record + 28) || !layer->width || layer->width > 256 || !layer->height ||
            layer->height > 192 || !layer->frames || layer->frames > 120 || !layer->hold ||
            layer->hold > SPLASH_TICK_CAP || layer->delay >= splash->duration ||
            layer->transition > splash->duration || (layer->effect && !layer->transition) ||
            (!motion && (layer->x < 0 || layer->y < 0 || layer->x + layer->width > 256 ||
                         layer->y + layer->height > 192)))
            return false;
        size_t frame_bytes = layer->width * layer->height * (layer->format ? 1u : 2u);
        size_t palette_bytes = layer->format ? 512u : 0u;
        bool packed = compressed && record[49];
        size_t bytes = read32(record + 24);
        if (read32(record + 20) != offset ||
            (!packed && bytes != frame_bytes * layer->frames + palette_bytes) ||
            bytes > splash->bytes - offset)
            return false;
        layer->palette = layer->format ? data + offset : NULL;
        if (packed) {
            size_t table_bytes = 4 * (layer->frames + 1);
            if (bytes < palette_bytes + table_bytes)
                return false;
            layer->frame_table = data + offset + palette_bytes;
            unsigned previous = (unsigned)table_bytes;
            if (read32(layer->frame_table) != previous)
                return false;
            for (unsigned frame = 0; frame < layer->frames; frame++) {
                unsigned end = read32(layer->frame_table + (frame + 1) * 4);
                if (end < previous || end - previous < 6 || end - previous > frame_bytes + 1024 ||
                    end > bytes - palette_bytes)
                    return false;
                previous = end;
            }
            if (previous != bytes - palette_bytes)
                return false;
            layer->decoded = malloc(frame_bytes);
            if (!layer->decoded)
                return false;
            layer->decoded_frame = UINT32_MAX;
            layer->pixels = layer->decoded;
        } else
            layer->pixels = data + offset + palette_bytes;
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
/* Motion uses fixed-point interpolation and bounded nearest-neighbour sampling.
 * No allocations, decoded frame copies or per-pixel divisions are required. */
static unsigned motion_progress(const SplashLayer *layer, unsigned elapsed)
{
    unsigned duration = layer->transition;
    if (!duration)
        return 1024;
    if (layer->repeat == 1)
        elapsed %= duration;
    else if (layer->repeat == 2) {
        elapsed %= duration * 2;
        if (elapsed > duration)
            elapsed = duration * 2 - elapsed;
    }
    unsigned p = elapsed >= duration ? 1024 : elapsed * 1024 / duration;
    if (layer->easing == 1)
        return p * p / 1024;
    if (layer->easing == 2)
        return 1024 - (1024 - p) * (1024 - p) / 1024;
    if (layer->easing == 3)
        return p * p * (3072 - 2 * p) / (1024 * 1024);
    return p;
}
static unsigned motion_frame(const SplashLayer *layer, unsigned elapsed)
{
    unsigned frame = elapsed / layer->hold;
    if (layer->sequence == 1)
        return frame % layer->frames;
    if (layer->sequence == 2 && layer->frames > 1) {
        unsigned cycle = (layer->frames - 1) * 2;
        frame %= cycle;
        return frame < layer->frames ? frame : cycle - frame;
    }
    return frame < layer->frames ? frame : layer->frames - 1;
}
bool splash_prepare(Splash *splash, unsigned ticks, bool reduced_motion)
{
    if (!splash->data)
        return false;
    if (reduced_motion)
        ticks = splash->poster;
    for (unsigned i = 0; i < splash->count; i++) {
        SplashLayer *layer = &splash->layers[i];
        if (!layer->decoded || (!reduced_motion && ticks < layer->delay))
            continue;
        unsigned elapsed = ticks > layer->delay ? ticks - layer->delay : 0;
        unsigned frame = motion_frame(layer, elapsed);
        if (layer->decoded_frame == frame)
            continue;
        layer->decoded_frame = UINT32_MAX;
        unsigned start = read32(layer->frame_table + frame * 4);
        unsigned end = read32(layer->frame_table + (frame + 1) * 4);
        size_t size = layer->width * layer->height * (layer->format ? 1u : 2u);
        if (!image_decode_zlib_frame(layer->decoded, size, layer->frame_table + start, end - start))
            return false;
        layer->decoded_frame = frame;
    }
    return true;
}
static int interpolate(int start, int end, unsigned progress)
{
    return start + (end - start) * (int)progress / 1024;
}
static void render_motion(const SplashLayer *layer, unsigned elapsed, bool reduced,
                          uint16_t *destination)
{
    unsigned progress = reduced ? 1024 : motion_progress(layer, elapsed);
    unsigned opacity = interpolate(layer->start_opacity, layer->end_opacity, progress);
    unsigned scale = interpolate(layer->start_scale, layer->end_scale, progress);
    int width = layer->width * scale / 100, height = layer->height * scale / 100;
    if (!width || !height || !opacity)
        return;
    /* Scaling keeps the layer's centre fixed; offsets are relative to its base position. */
    int x = layer->x + ((int)layer->width - width) / 2 +
            interpolate(layer->start_x, layer->end_x, progress);
    int y = layer->y + ((int)layer->height - height) / 2 +
            interpolate(layer->start_y, layer->end_y, progress);
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    int right = x + width > 256 ? 256 : x + width;
    int bottom = y + height > 192 ? 192 : y + height;
    /* Reveal directions: right, left, up and down. */
    int visible_width = width * progress / 1024, visible_height = height * progress / 1024;
    if (layer->effect == 1 && right > x + visible_width)
        right = x + visible_width;
    if (layer->effect == 2 && left < x + width - visible_width)
        left = x + width - visible_width;
    if (layer->effect == 3 && top < y + height - visible_height)
        top = y + height - visible_height;
    if (layer->effect == 4 && bottom > y + visible_height)
        bottom = y + visible_height;
    if (left >= right || top >= bottom)
        return;
    unsigned columns[256];
    for (int column = left; column < right; column++)
        columns[column] = (column - x) * layer->width / width;
    unsigned frame = motion_frame(layer, elapsed);
    if (layer->decoded) {
        if (layer->decoded_frame != frame)
            return;
        frame = 0;
    }
    for (int row = top; row < bottom; row++) {
        size_t offset = (frame * layer->height + (row - y) * layer->height / height) * layer->width;
        for (int column = left; column < right; column++) {
            size_t pixel = offset + columns[column];
            uint16_t color = layer->format ? read16(layer->palette + layer->pixels[pixel] * 2)
                                           : read16(layer->pixels + pixel * 2);
            if (color & 0x8000) {
                uint16_t *out = destination + row * 256 + column;
                *out = opacity == 32 ? color : blend(color, *out, opacity);
            }
        }
    }
}

static unsigned layer_state(const SplashLayer *layer, unsigned ticks)
{
    if (ticks < layer->delay)
        return UINT32_MAX;
    unsigned elapsed = ticks - layer->delay;
    if (layer->motion) {
        bool moving = layer->effect || layer->start_x != layer->end_x ||
                      layer->start_y != layer->end_y || layer->start_scale != layer->end_scale ||
                      layer->start_opacity != layer->end_opacity;
        return motion_frame(layer, elapsed) * 2048 + (moving ? motion_progress(layer, elapsed) : 0);
    }
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
        top[p] = bottom[p] = splash->data ? splash->background : 0x8000;
    if (!splash->data)
        return;
    if (reduced_motion)
        ticks = splash->poster;
    for (unsigned i = 0; i < splash->count; i++) {
        const SplashLayer *layer = &splash->layers[i];
        if (!reduced_motion && ticks < layer->delay)
            continue;
        unsigned elapsed = ticks > layer->delay ? ticks - layer->delay : 0;
        if (layer->motion) {
            render_motion(layer, elapsed, reduced_motion, layer->screen ? bottom : top);
            continue;
        }
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
