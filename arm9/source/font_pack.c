/* Bounded bitmap metrics; the recovery face is always available without I/O. */
#include "font_pack.h"
#include "text.h"
#include "ui/font.h"
#include "ui/font_extended.h"
#include "ui/font_latin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const FontPack *current;

void font_builtin(FontPack *font)
{
    memset(font, 0, sizeof *font);
    for (int glyph = 0; glyph < FONT_GLYPHS; glyph++)
        for (int row = 0; row < FONT_HEIGHT; row++)
            font->rows[glyph][row] = glyph < FONT_ASCII_GLYPHS
                                         ? ui_font[glyph][row]
                                         : ui_font_latin[glyph - FONT_ASCII_GLYPHS][row];
    memset(font->advance, FONT_WIDTH, sizeof font->advance);
    font->width = FONT_WIDTH;
    font->height = FONT_HEIGHT;
    font->glyph_count = FONT_GLYPHS;
}
void font_use(const FontPack *font)
{
    current = font;
}
const FontPack *font_current(void)
{
    return current;
}
unsigned font_height(const FontPack *font)
{
    return font ? font->height : FONT_HEIGHT;
}

static unsigned extended_index(unsigned character)
{
    unsigned index = 528;
    if (character >= 0x100 && character < 0x180)
        index = character - 0x100;
    else if (character >= 0x370 && character < 0x500)
        index = 128 + character - 0x370;
    return index < 528 && ui_font_extended_supported[index] ? index : 528;
}

unsigned font_advance(const FontPack *font, unsigned character)
{
    if (!font || extended_index(character) < 528)
        return FONT_WIDTH;
    unsigned glyph;
    if (character >= 160 && character <= 255) {
        if (font->glyph_count != FONT_GLYPHS)
            return FONT_WIDTH;
        glyph = FONT_ASCII_GLYPHS + character - 160;
    } else {
        if (character < 32 || character > 126)
            character = '?';
        glyph = character - 32;
    }
    return font->advance[glyph];
}

uint32_t font_glyph_row(const FontPack *font, unsigned character, int row)
{
    if (row < 0 || row >= (int)font_height(font))
        return 0;
    unsigned extended = extended_index(character);
    int fallback_row = row - ((int)font_height(font) - FONT_HEIGHT);
    if (extended < 528)
        return fallback_row >= 0 ? ui_font_extended[extended][fallback_row] : 0;
    if (character >= 160 && character <= 255) {
        unsigned glyph = FONT_ASCII_GLYPHS + character - 160;
        return font && font->glyph_count == FONT_GLYPHS ? font->rows[glyph][row]
               : fallback_row >= 0 ? ui_font_latin[character - 160][fallback_row]
                                   : 0;
    }
    if (character < 32 || character > 126)
        character = '?';
    return font ? font->rows[character - 32][row] : ui_font[character - 32][row];
}
uint32_t font_row(unsigned character, int row)
{
    return font_glyph_row(current, character, row);
}
unsigned font_measure(const FontPack *font, const char *value)
{
    unsigned width = 0;
    while (value && *value)
        width += font_advance(font, text_next(&value));
    return width;
}

bool font_pack_load(FontPack *font, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char header[16];
    static const unsigned char ascii[16] = {'D', 'S', 'F', 'O', 'N', 'T', '0', '1',
                                            7,   12,  32,  95,  0,   0,   0,   0};
    static const unsigned char latin[16] = {'D', 'S', 'F', 'O', 'N', 'T', '0', '2',
                                            7,   12,  32,  95,  160, 96,  0,   0};
    bool valid = fread(header, 1, 16, file) == 16;
    bool proportional = valid && !memcmp(header, "DSFONT03", 8);
    bool extended = valid && (proportional ? header[13] == 96 : !memcmp(header, latin, 16));
    if (proportional)
        valid = header[8] >= 1 && header[8] <= FONT_MAX_WIDTH && header[9] >= FONT_HEIGHT &&
                header[9] <= FONT_MAX_HEIGHT && header[10] == 32 && header[11] == 95 &&
                header[12] == (extended ? 160 : 0) && header[13] == (extended ? 96 : 0) &&
                !header[14] && !header[15];
    else
        valid = valid && (extended || !memcmp(header, ascii, 16));
    /* Transactional loading without putting a full two-role face on the ARM9 stack. */
    FontPack *candidate = valid ? calloc(1, sizeof *candidate) : NULL;
    valid = valid && candidate;
    if (valid) {
        candidate->glyph_count = extended ? FONT_GLYPHS : FONT_ASCII_GLYPHS;
        candidate->width = header[8];
        candidate->height = header[9];
        candidate->proportional = proportional;
    }
    for (unsigned glyph = 0; valid && glyph < candidate->glyph_count; glyph++) {
        bool visible = false;
        unsigned advance = FONT_WIDTH;
        if (proportional) {
            int a = fgetc(file), reserved = fgetc(file);
            valid = a >= 1 && a <= candidate->width && reserved == 0;
            advance = valid ? (unsigned)a : FONT_WIDTH;
        }
        candidate->advance[glyph] = advance;
        for (unsigned row = 0; valid && row < candidate->height; row++) {
            unsigned char bytes[4] = {0};
            unsigned count = proportional ? 4 : 2;
            if (fread(bytes, 1, count, file) != count) {
                valid = false;
                break;
            }
            uint32_t bits = bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
                            ((uint32_t)bytes[3] << 24);
            if (advance < 16 && (bits >> (advance * 2)))
                valid = false;
            candidate->rows[glyph][row] = bits;
            visible |= bits != 0;
        }
        if (glyph != 0 && glyph != 95 && glyph != 108 && !visible)
            valid = false;
    }
    valid = valid && fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (valid)
        *font = *candidate;
    free(candidate);
    return valid;
}
