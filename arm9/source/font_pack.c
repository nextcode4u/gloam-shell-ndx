/* Fixed-cell font packs deliberately share the recovery font's exact envelope. */
#include "font_pack.h"
#include "ui/font.h"
#include "ui/font_latin.h"
#include "ui/font_extended.h"
#include <stdio.h>
#include <string.h>

static const FontPack *current;
void font_builtin(FontPack *font)
{
    memset(font, 0, sizeof *font);
    memcpy(font->rows, ui_font, sizeof ui_font);
    memcpy(font->rows + FONT_ASCII_GLYPHS, ui_font_latin, sizeof ui_font_latin);
    font->glyph_count = FONT_GLYPHS;
}
void font_use(const FontPack *font)
{
    current = font;
}
uint16_t font_row(unsigned character, int row)
{
    if (row < 0 || row >= FONT_HEIGHT)
        return 0;
    unsigned extended = 528;
    if (character >= 0x100 && character < 0x180)
        extended = character - 0x100;
    else if (character >= 0x370 && character < 0x500)
        extended = 128 + character - 0x370;
    if (extended < 528 && ui_font_extended_supported[extended])
        return ui_font_extended[extended][row];
    if (character >= 160 && character <= 255) {
        unsigned glyph = FONT_ASCII_GLYPHS + character - 160;
        return current && current->glyph_count == FONT_GLYPHS ? current->rows[glyph][row]
                                                              : ui_font_latin[character - 160][row];
    }
    if (character < 32 || character > 126)
        character = '?';
    return current ? current->rows[character - 32][row] : ui_font[character - 32][row];
}

bool font_pack_load(FontPack *font, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char header[16];
    static const unsigned char expected[16] = {'D', 'S', 'F', 'O', 'N', 'T', '0', '1',
                                               7,   12,  32,  95,  0,   0,   0,   0};
    static const unsigned char extended[16] = {'D', 'S', 'F', 'O', 'N', 'T', '0', '2',
                                               7,   12,  32,  95,  160, 96,  0,   0};
    bool valid = fread(header, 1, 16, file) == 16;
    bool latin = valid && !memcmp(header, extended, 16);
    valid = valid && (latin || !memcmp(header, expected, 16));
    FontPack candidate = {0};
    candidate.glyph_count = latin ? FONT_GLYPHS : FONT_ASCII_GLYPHS;
    for (int glyph = 0; valid && glyph < candidate.glyph_count; glyph++) {
        bool visible = false;
        for (int row = 0; row < FONT_HEIGHT; row++) {
            unsigned char bytes[2];
            if (fread(bytes, 1, 2, file) != 2) {
                valid = false;
                break;
            }
            uint16_t bits = bytes[0] | ((uint16_t)bytes[1] << 8);
            if (bits & 0xC000)
                valid = false;
            candidate.rows[glyph][row] = bits;
            visible |= bits != 0;
        }
        if (glyph != 0 && glyph != 95 && glyph != 108 && !visible)
            valid = false;
    }
    valid = valid && fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (valid)
        *font = candidate;
    return valid;
}
