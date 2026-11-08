#ifndef FRONTEND_FONT_PACK_H
#define FRONTEND_FONT_PACK_H
#include <stdbool.h>
#include <stdint.h>
#define FONT_WIDTH 7
#define FONT_HEIGHT 12
#define FONT_ASCII_GLYPHS 95
#define FONT_GLYPHS 191
typedef struct {
    uint16_t rows[FONT_GLYPHS][FONT_HEIGHT];
    unsigned short glyph_count;
} FontPack;
void font_builtin(FontPack *font);
bool font_pack_load(FontPack *font, const char *path);
/* The controller selects the active machine-wide face; rendering only reads it. */
void font_use(const FontPack *font);
uint16_t font_row(unsigned character, int row);
#endif
