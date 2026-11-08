/* Small, clipped RGB555 drawing primitives; no filesystem or hardware access. */
#include "drawing.h"
#include "font_pack.h"
#include "text.h"
#define UI_FONT_HEIGHT FONT_HEIGHT
#define UI_FONT_ADVANCE FONT_WIDTH
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

/* Blend coverage against the actual surface, including selected rows. */
static uint16_t blend_text_pixel(uint16_t foreground, uint16_t background, unsigned coverage)
{
    unsigned inverse = 3 - coverage;
    unsigned red = (foreground & 31) * coverage + (background & 31) * inverse + 1;
    unsigned green = ((foreground >> 5) & 31) * coverage + ((background >> 5) & 31) * inverse + 1;
    unsigned blue = ((foreground >> 10) & 31) * coverage + ((background >> 10) & 31) * inverse + 1;
    /* Each numerator is 0..94. In this range (n * 171) >> 9 is exactly n / 3,
     * avoiding three ARM9 Thumb software divisions per antialiased pixel. */
    red = red * 171 >> 9;
    green = green * 171 >> 9;
    blue = blue * 171 >> 9;
    return (uint16_t)(0x8000 | red | (green << 5) | (blue << 10));
}

void draw_text(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
               int max_characters)
{
    for (int index = 0; value && *value && index < max_characters; index++) {
        unsigned character = text_next(&value);
        for (int row = 0; row < UI_FONT_HEIGHT; row++) {
            int screen_y = y + row;
            if (screen_y < 0 || screen_y >= 192)
                continue;
            unsigned bits = font_row(character, row);
            for (int column = 0; column < UI_FONT_ADVANCE; column++) {
                int screen_x = x + index * UI_FONT_ADVANCE + column;
                unsigned coverage = (bits >> (column * 2)) & 3;
                if (screen_x < 0 || screen_x >= 256 || coverage == 0)
                    continue;
                uint16_t *destination = &pixels[screen_y * 256 + screen_x];
                *destination =
                    coverage == 3 ? color : blend_text_pixel(color, *destination, coverage);
            }
        }
    }
}

void draw_text_clipped(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
                       int max_characters)
{
    char clipped[512];
    int available_characters = (256 - x) / UI_FONT_ADVANCE;
    if (max_characters > available_characters)
        max_characters = available_characters;
    if (max_characters > 120)
        max_characters = 120;
    if (max_characters <= 0)
        return;
    bool shortened = text_length(value) > (size_t)max_characters;
    int kept = shortened && max_characters > 2 ? max_characters - 2 : max_characters;
    size_t bytes = text_prefix_bytes(value, kept);
    memcpy(clipped, value ? value : "", bytes);
    clipped[bytes] = 0;
    if (shortened && max_characters > 2)
        strcat(clipped, "..");
    draw_text(pixels, x, y, clipped, color, max_characters);
}

void draw_text_scrolling(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
                         int width, unsigned elapsed_frames, bool animate)
{
    if (!value || width <= 0)
        return;
    unsigned text_width = (unsigned)text_length(value) * UI_FONT_ADVANCE;
    if (!animate || text_width <= (unsigned)width) {
        draw_text_clipped(pixels, x, y, value, color, width / UI_FONT_ADVANCE);
        return;
    }
    /* Move immediately at 20 pixels/second. Three
     * blank cells separate repetitions; the next beginning follows the tail
     * through the same viewport, without an end stop or a position jump. */
    unsigned length = text_width / UI_FONT_ADVANCE;
    unsigned cycle_cells = length + 3;
    unsigned offset = (elapsed_frames / 3) % (cycle_cells * UI_FONT_ADVANCE);
    unsigned cell = offset / UI_FONT_ADVANCE;
    int left = x < 0 ? 0 : x;
    int right = x + width > 256 ? 256 : x + width;
    const char *cursor = value + text_prefix_bytes(value, cell < length ? cell : length);
    int origin = x - (int)(offset % UI_FONT_ADVANCE);
    for (; origin < right; origin += UI_FONT_ADVANCE) {
        unsigned character = cell < length ? text_next(&cursor) : ' ';
        if (++cell == cycle_cells) {
            cell = 0;
            cursor = value;
        }
        for (int row = 0; row < UI_FONT_HEIGHT; row++) {
            int screen_y = y + row;
            if (screen_y < 0 || screen_y >= 192)
                continue;
            unsigned bits = font_row(character, row);
            for (int column = 0; column < UI_FONT_ADVANCE; column++) {
                int screen_x = origin + column;
                unsigned coverage = (bits >> (column * 2)) & 3;
                if (!coverage || screen_x < left || screen_x >= right)
                    continue;
                uint16_t *destination = &pixels[screen_y * 256 + screen_x];
                *destination =
                    coverage == 3 ? color : blend_text_pixel(color, *destination, coverage);
            }
        }
    }
}

void draw_text_wrapped(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
                       int line_width, int max_lines)
{
    int available_characters = (256 - x - 10) / UI_FONT_ADVANCE;
    if (line_width > available_characters)
        line_width = available_characters;
    if (line_width > 120)
        line_width = 120;
    if (line_width <= 0 || !value)
        return;
    const char *cursor = value;
    for (int row = 0; row < max_lines && *cursor; row++) {
        const char *end = cursor, *word_break = NULL;
        for (int column = 0; column < line_width && *end && *end != '\n'; column++) {
            if (*end == ' ' && column > line_width / 2)
                word_break = end;
            text_next(&end);
        }
        if (*end && *end != '\n' && word_break)
            end = word_break;
        char line[512];
        size_t bytes = (size_t)(end - cursor);
        memcpy(line, cursor, bytes);
        line[bytes] = 0;
        draw_text(pixels, x, y + row * 12, line, color, line_width);
        cursor = end;
        if (*cursor == '\n')
            cursor++;
        while (*cursor == ' ')
            cursor++;
    }
}
