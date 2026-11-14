/* Small, clipped RGB555 drawing primitives; no filesystem or hardware access. */
#include "drawing.h"
#include "font_pack.h"
#include "text.h"
#include <stdio.h>
#include <string.h>

uint16_t draw_rgb555(int r, int g, int b)
{
    return (uint16_t)(0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
}

void draw_rectangle(uint16_t *pixels, int x, int y, int width, int height, uint16_t color)
{
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x + width > 256)
        width = 256 - x;
    if (y + height > 192)
        height = 192 - y;
    if (width <= 0 || height <= 0)
        return;
    uint16_t *first = pixels + y * 256 + x;
    for (int column = 0; column < width; column++)
        first[column] = color;
    /* Reuse the completed row through the CPU library's optimized copy.
     * Clipping guarantees distinct, non-overlapping rows, including odd x. */
    for (int row = 1; row < height; row++)
        memcpy(first + row * 256, first, (size_t)width * sizeof *first);
}

unsigned draw_text_width(const char *value)
{
    return font_measure(font_current(), value);
}

static void glyph(uint16_t *pixels, const FontPack *font, int x, int y, unsigned character,
                  uint16_t color, int left, int right)
{
    unsigned advance = font_advance(font, character);
    for (unsigned row = 0; row < font_height(font); row++) {
        int screen_y = y + row;
        if (screen_y < 0 || screen_y >= 192)
            continue;
        uint32_t bits = font_glyph_row(font, character, row);
        for (unsigned column = 0; column < advance; column++) {
            int screen_x = x + column;
            unsigned coverage = (bits >> (column * 2)) & 3;
            if (screen_x < left || screen_x >= right || !coverage)
                continue;
            uint16_t *destination = &pixels[screen_y * 256 + screen_x];
            *destination =
                coverage == 3 ? color : draw_blend_coverage(color, *destination, coverage);
        }
    }
}

static void text_fit(uint16_t *pixels, const FontPack *font, int x, int y, const char *value,
                     uint16_t color, int width, bool ellipsis)
{
    if (!value || width <= 0 || x >= 256 || x + width <= 0)
        return;
    if (width > 256 - x)
        width = 256 - x;
    int left = x < 0 ? 0 : x, right = x + width;
    unsigned dots = font_advance(font, '.') * 2;
    bool shortened = ellipsis && font_measure(font, value) > (unsigned)width && width > (int)dots;
    int limit = width - (shortened ? (int)dots : 0), used = 0;
    while (*value) {
        unsigned character = text_next(&value), advance = font_advance(font, character);
        if (used + (int)advance > limit)
            break;
        glyph(pixels, font, x + used, y, character, color, left, right);
        used += advance;
    }
    if (shortened) {
        glyph(pixels, font, x + used, y, '.', color, left, right);
        glyph(pixels, font, x + used + dots / 2, y, '.', color, left, right);
    }
}

static void text_scrolling(uint16_t *pixels, const FontPack *font, int x, int y, const char *value,
                           uint16_t color, int width, unsigned elapsed_frames, bool animate)
{
    if (!value || width <= 0 || x >= 256 || x + width <= 0)
        return;
    unsigned text_width = font_measure(font, value);
    if (!animate || text_width <= (unsigned)width) {
        text_fit(pixels, font, x, y, value, color, width, true);
        return;
    }
    unsigned gap = 3 * font_advance(font, ' ');
    unsigned offset = (elapsed_frames / 3) % (text_width + gap);
    int left = x < 0 ? 0 : x, right = x + width > 256 ? 256 : x + width;
    int origin = x - (int)offset;
    const char *cursor = value;
    /* Pixel distances, including the loop gap, use the very same advances as drawing. */
    while (origin < right) {
        if (!*cursor) {
            origin += gap;
            cursor = value;
            continue;
        }
        unsigned character = text_next(&cursor), advance = font_advance(font, character);
        if (origin + (int)advance > left)
            glyph(pixels, font, origin, y, character, color, left, right);
        origin += advance;
    }
}

void draw_text(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
               int max_characters)
{
    if (max_characters > 120)
        max_characters = 120;
    text_fit(pixels, font_current(), x, y, value, color, max_characters * FONT_WIDTH, false);
}
void draw_text_clipped(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
                       int max_characters)
{
    if (max_characters > 120)
        max_characters = 120;
    text_fit(pixels, font_current(), x, y, value, color, max_characters * FONT_WIDTH, true);
}
void draw_text_scrolling(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
                         int width, unsigned elapsed_frames, bool animate)
{
    text_scrolling(pixels, font_current(), x, y, value, color, width, elapsed_frames, animate);
}

void draw_heading_scrolling(const ThemePackage *package, uint16_t *pixels, int x, int y,
                            const char *value, uint16_t color, int width, unsigned elapsed_frames,
                            bool animate)
{
    const FontPack *font = package->custom_heading ? &package->heading_font : &package->font;
    /* Headings occupy at most two extra pixels above/below the existing 12px line. */
    y -= ((int)font_height(font) - FONT_HEIGHT) / 2;
    text_scrolling(pixels, font, x, y, value, color, width, elapsed_frames, animate);
}
void draw_heading(const ThemePackage *package, uint16_t *pixels, int x, int y, const char *value,
                  uint16_t color, int width)
{
    draw_heading_scrolling(package, pixels, x, y, value, color, width, 0, false);
}

void draw_text_wrapped(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
                       int line_width, int max_lines)
{
    if (line_width > 120)
        line_width = 120;
    int width = line_width * FONT_WIDTH;
    if (width > 256 - x - 10)
        width = 256 - x - 10;
    if (width <= 0 || !value)
        return;
    const FontPack *font = font_current();
    const char *cursor = value;
    int left = x < 0 ? 0 : x, right = x + width;
    for (int row = 0; row < max_lines && *cursor; row++) {
        const char *end = cursor, *word_break = NULL;
        unsigned used = 0;
        while (*end && *end != '\n') {
            const char *next = end;
            unsigned advance = font_advance(font, text_next(&next));
            if (used + advance > (unsigned)width)
                break;
            if (*end == ' ')
                word_break = end;
            used += advance;
            end = next;
        }
        if (*end && *end != '\n' && word_break && word_break > cursor)
            end = word_break;
        if (end == cursor && *end != '\n')
            text_next(&end); /* Even a narrower-than-one-glyph column must advance. */
        int pen = x;
        while (cursor < end) {
            unsigned character = text_next(&cursor);
            glyph(pixels, font, pen, y + row * FONT_HEIGHT, character, color, left, right);
            pen += font_advance(font, character);
        }
        if (*cursor == '\n')
            cursor++;
        while (*cursor == ' ')
            cursor++;
    }
}
