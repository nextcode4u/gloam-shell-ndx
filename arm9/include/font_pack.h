#ifndef FRONTEND_FONT_PACK_H
#define FRONTEND_FONT_PACK_H
#include <stdbool.h>
#include <stdint.h>
#define FONT_WIDTH 7
#define FONT_HEIGHT 12
#define FONT_MAX_WIDTH 16
#define FONT_MAX_HEIGHT 16
#define FONT_ASCII_GLYPHS 95
#define FONT_GLYPHS 191
typedef struct {
    uint32_t rows[FONT_GLYPHS][FONT_MAX_HEIGHT];
    uint8_t advance[FONT_GLYPHS];
    unsigned short glyph_count;
    uint8_t width, height;
    bool proportional;
} FontPack;
void font_builtin(FontPack *font);
bool font_pack_load(FontPack *font, const char *path);
/* The controller selects the active machine-wide face; rendering only reads it. */
void font_use(const FontPack *font);
const FontPack *font_current(void);
unsigned font_height(const FontPack *font);
unsigned font_advance(const FontPack *font, unsigned character);
unsigned font_measure(const FontPack *font, const char *value);
uint32_t font_glyph_row(const FontPack *font, unsigned character, int row);
uint32_t font_row(unsigned character, int row);
#endif
