/* Optional machine-wide script faces, independent of the active theme. */
#include "script_fonts.h"
#include "paths.h"
#include "text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCRIPT_HEIGHT 12
#define SCRIPT_RECORD_BYTES (5 + SCRIPT_HEIGHT * 2)
#define SCRIPT_GLYPH_LIMIT 16384

typedef struct {
    const char *file;
    unsigned first, last;
    unsigned char *data;
    unsigned count, depth;
    bool attempted;
} ScriptFont;
static ScriptFont packs[] = {{.file = "extended.gsf", .first = 0x100, .last = 0x4ff},
                             {.file = "japanese.gsf", .first = 0x2000, .last = 0xffff}};
static char directory[PATH_CAP];

static uint32_t word(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static unsigned row_bits(const unsigned char *p)
{
    return p[0] | p[1] << 8;
}
static uint32_t checksum(const unsigned char *data, size_t size)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; i++)
        hash = (hash ^ data[i]) * 16777619u;
    return hash;
}
void script_fonts_configure(const char *root, bool enabled)
{
    for (unsigned i = 0; i < sizeof packs / sizeof *packs; i++) {
        free(packs[i].data);
        packs[i].data = NULL;
        packs[i].count = packs[i].depth = 0;
        packs[i].attempted = false;
    }
    directory[0] = 0;
    if (enabled && !path_join(directory, root, "_nds/dsi-frontend/fonts"))
        directory[0] = 0;
}
static void load_pack(ScriptFont *pack)
{
    pack->attempted = true;
    char path[PATH_CAP];
    if (!*directory || !path_join(path, directory, pack->file))
        return;
    FILE *file = fopen(path, "rb");
    if (!file)
        return;
    unsigned char header[20];
    bool valid = fread(header, 1, sizeof header, file) == sizeof header &&
                 !memcmp(header, "GSFONT01", 8) && header[16] == SCRIPT_HEIGHT &&
                 (header[17] == 1 || header[17] == 2) && !header[18] && !header[19];
    unsigned count = valid ? word(header + 8) : 0;
    valid = valid && count && count <= SCRIPT_GLYPH_LIMIT;
    size_t size = (size_t)count * SCRIPT_RECORD_BYTES;
    unsigned char *data = valid ? malloc(size) : NULL;
    valid =
        valid && data && fread(data, 1, size, file) == size && fgetc(file) == EOF && !ferror(file);
    if (fclose(file))
        valid = false;
    if (valid)
        valid = checksum(data, size) == word(header + 12);
    unsigned previous = 0;
    for (unsigned i = 0; valid && i < count; i++) {
        const unsigned char *glyph = data + i * SCRIPT_RECORD_BYTES;
        unsigned cp = word(glyph), advance = glyph[4];
        valid = cp >= pack->first && cp <= pack->last && cp > previous &&
                !(cp >= 0xd800 && cp <= 0xdfff) && advance && advance * header[17] <= 16;
        for (unsigned row = 0; valid && row < SCRIPT_HEIGHT; row++)
            valid = !(row_bits(glyph + 5 + row * 2) >> (advance * header[17]));
        previous = cp;
    }
    if (!valid) {
        free(data);
        return;
    }
    pack->data = data;
    pack->count = count;
    pack->depth = header[17];
}
void script_fonts_prepare_text(const char *text)
{
    if (!*directory)
        return;
    while (text && *text) {
        unsigned cp = text_next(&text);
        for (unsigned i = 0; i < sizeof packs / sizeof *packs; i++)
            if (!packs[i].attempted && cp >= packs[i].first && cp <= packs[i].last)
                load_pack(&packs[i]);
    }
}
bool script_fonts_glyph(unsigned character, int row, unsigned *advance, uint32_t *coverage)
{
    *coverage = 0;
    for (unsigned i = 0; i < sizeof packs / sizeof *packs; i++) {
        const ScriptFont *pack = &packs[i];
        if (!pack->data || character < pack->first || character > pack->last)
            continue;
        unsigned low = 0, high = pack->count;
        while (low < high) {
            unsigned middle = low + (high - low) / 2;
            const unsigned char *glyph = pack->data + middle * SCRIPT_RECORD_BYTES;
            unsigned cp = word(glyph);
            if (cp < character)
                low = middle + 1;
            else if (cp > character)
                high = middle;
            else {
                *advance = glyph[4];
                if (row >= 0 && row < SCRIPT_HEIGHT) {
                    unsigned bits = row_bits(glyph + 5 + row * 2);
                    if (pack->depth == 2)
                        *coverage = bits;
                    else
                        for (unsigned x = 0; x < *advance; x++)
                            *coverage |= ((bits >> x) & 1) * (3u << (x * 2));
                }
                return true;
            }
        }
    }
    return false;
}
size_t script_fonts_bytes(void)
{
    size_t bytes = 0;
    for (unsigned i = 0; i < sizeof packs / sizeof *packs; i++)
        bytes += packs[i].count * SCRIPT_RECORD_BYTES;
    return bytes;
}
unsigned script_fonts_count(void)
{
    unsigned count = 0;
    for (unsigned i = 0; i < sizeof packs / sizeof *packs; i++)
        count += packs[i].count;
    return count;
}
unsigned script_fonts_attempted(void)
{
    unsigned count = 0;
    for (unsigned i = 0; i < sizeof packs / sizeof *packs; i++)
        count += packs[i].attempted;
    return count;
}
