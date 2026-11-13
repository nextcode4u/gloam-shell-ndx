/* Original, palette-tinted 16x16 symbols shared by every placeholder surface. */
#include "drawing.h"
#include "themes.h"

static const uint16_t glyphs[PLACEHOLDER_COUNT][16] = {
    [PLACEHOLDER_FILE] = {0x0000, 0x3fc0, 0x2060, 0x2050, 0x2048, 0x2078, 0x2008, 0x2008, 0x2008,
                          0x2008, 0x2008, 0x2008, 0x2008, 0x3ff8, 0x0000, 0x0000},
    [PLACEHOLDER_FOLDER] = {0x0000, 0x0000, 0x3e00, 0x7f00, 0x7ffc, 0x4004, 0x4004, 0x4004, 0x4004,
                            0x4004, 0x4004, 0x4004, 0x7ffc, 0x0000, 0x0000, 0x0000},
    [PLACEHOLDER_GAME] = {0x0000, 0x0000, 0x1ff8, 0x3008, 0x27e8, 0x2428, 0x27e8, 0x2008, 0x2008,
                          0x2a28, 0x3b78, 0x2a28, 0x2008, 0x3ff8, 0x0000, 0x0000},
    [PLACEHOLDER_APPLICATION] = {0x0000, 0x0000, 0x7ffe, 0x4002, 0x5552, 0x7ffe, 0x4002, 0x4cc2,
                                 0x4cc2, 0x4002, 0x4cc2, 0x4cc2, 0x4002, 0x7ffe, 0x0000, 0x0000},
    [PLACEHOLDER_AUDIO] = {0x0000, 0x0000, 0x01f8, 0x07f8, 0x0618, 0x0618, 0x0618, 0x0618, 0x0618,
                           0x0678, 0x1e78, 0x3e70, 0x3c00, 0x1800, 0x0000, 0x0000},
    [PLACEHOLDER_VIDEO] = {0x0000, 0x0000, 0x7ffe, 0x6aaa, 0x4002, 0x4182, 0x41c2, 0x41e2, 0x41c2,
                           0x4182, 0x4002, 0x6aaa, 0x7ffe, 0x0000, 0x0000, 0x0000},
    [PLACEHOLDER_BOOK] = {0x0000, 0x0000, 0x3ef8, 0x630c, 0x4184, 0x4184, 0x5d74, 0x4184, 0x5d74,
                          0x4184, 0x4184, 0x630c, 0x3ef8, 0x0180, 0x0000, 0x0000},
    [PLACEHOLDER_TEXT] = {0x0000, 0x3fc0, 0x2060, 0x2050, 0x2048, 0x2078, 0x2008, 0x2fa8, 0x2008,
                          0x2fa8, 0x2008, 0x2f08, 0x2008, 0x3ff8, 0x0000, 0x0000},
    [PLACEHOLDER_IMAGE] = {0x0000, 0x0000, 0x7ffe, 0x4002, 0x4c02, 0x4c02, 0x4002, 0x4062, 0x40f2,
                           0x498a, 0x5b06, 0x7602, 0x4002, 0x7ffe, 0x0000, 0x0000},
    [PLACEHOLDER_SAVE] = {0x0000, 0x0000, 0x7ffc, 0x4246, 0x4242, 0x43c2, 0x4002, 0x4002, 0x4ff2,
                          0x4812, 0x4bd2, 0x4812, 0x4ff2, 0x7ffe, 0x0000, 0x0000},
    [PLACEHOLDER_CONFIG] = {0x0000, 0x0000, 0x1008, 0x17fe, 0x1008, 0x0040, 0x7ff0, 0x0040, 0x0200,
                            0x7f7e, 0x0200, 0x0008, 0x7ffe, 0x0008, 0x0000, 0x0000},
    [PLACEHOLDER_ARCHIVE] = {0x0000, 0x3fc0, 0x2260, 0x2150, 0x2248, 0x2178, 0x2208, 0x2108, 0x2208,
                             0x2108, 0x2388, 0x2288, 0x2388, 0x3ff8, 0x0000, 0x0000},
    [PLACEHOLDER_SHORTCUT] = {0x0000, 0x3fc0, 0x2060, 0x2050, 0x2048, 0x2078, 0x2008, 0x2188,
                              0x23c8, 0x27e8, 0x2188, 0x2f88, 0x2008, 0x3ff8, 0x0000, 0x0000},
    [PLACEHOLDER_SYSTEM] = {0x03c0, 0x03c0, 0x3bdc, 0x3bdc, 0x3ffc, 0x0e70, 0xfc3f, 0xf81f, 0xf81f,
                            0xfc3f, 0x0e70, 0x3ffc, 0x3bdc, 0x3bdc, 0x03c0, 0x03c0},
    [PLACEHOLDER_MISSING] = {0x0000, 0x3fc0, 0x2060, 0x2050, 0x2048, 0x2078, 0x2008, 0x2448, 0x2288,
                             0x2108, 0x2288, 0x2448, 0x2008, 0x3ff8, 0x0000, 0x0000},
};
void draw_placeholder_icon(uint16_t *pixels, int x, int y, int size, PlaceholderIcon icon,
                           uint16_t color)
{
    if (size <= 0 || size > 96)
        return;
    if ((unsigned)icon >= PLACEHOLDER_COUNT)
        icon = PLACEHOLDER_FILE;
    for (int row = 0; row < 16; row++) {
        int top = y + row * size / 16, bottom = y + (row + 1) * size / 16;
        for (int column = 0; column < 16; column++) {
            if (!(glyphs[icon][row] & (0x8000u >> column)))
                continue;
            int start = column;
            while (column + 1 < 16 && (glyphs[icon][row] & (0x8000u >> (column + 1))))
                column++;
            int left = x + start * size / 16, right = x + (column + 1) * size / 16;
            draw_rectangle(pixels, left, top, right - left, bottom - top, color);
        }
    }
}

/* Nearest-neighbour scaling keeps small pixel art crisp; clip every destination. */
bool draw_theme_icon_asset(const ThemePackage *package, uint16_t *pixels, int x, int y, int size,
                           int icon)
{
    if (size <= 0 || size > 96)
        return false;
    const uint16_t *image = themes_icon(package, icon);
    if (!image)
        return false;
    for (int row = 0; row < size; row++) {
        if (y + row < 0 || y + row >= 192)
            continue;
        for (int column = 0; column < size; column++) {
            if (x + column < 0 || x + column >= 256)
                continue;
            uint16_t pixel = image[(row * 32 / size) * 32 + column * 32 / size];
            if (pixel & 0x8000)
                pixels[(y + row) * 256 + x + column] = pixel;
        }
    }
    return true;
}

void draw_theme_icon(const ThemePackage *package, uint16_t *pixels, int x, int y, int size,
                     PlaceholderIcon icon, uint16_t color)
{
    if (!draw_theme_icon_asset(package, pixels, x, y, size, icon))
        draw_placeholder_icon(pixels, x, y, size, icon, color);
}
