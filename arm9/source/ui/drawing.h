#ifndef FRONTEND_UI_DRAWING_H
#define FRONTEND_UI_DRAWING_H
#include "file_types.h"
#include "themes.h"
#include <stdbool.h>
#include <stdint.h>
/* Four-level coverage against the actual destination. Numerators stay below 95,
 * so multiplication replaces ARM9 software division without rounding changes. */
static inline uint16_t draw_blend_coverage(uint16_t foreground, uint16_t background, unsigned alpha)
{
    unsigned inverse = 3 - alpha;
    unsigned red = ((foreground & 31) * alpha + (background & 31) * inverse + 1) * 171 >> 9;
    unsigned green =
        (((foreground >> 5) & 31) * alpha + ((background >> 5) & 31) * inverse + 1) * 171 >> 9;
    unsigned blue =
        (((foreground >> 10) & 31) * alpha + ((background >> 10) & 31) * inverse + 1) * 171 >> 9;
    return (uint16_t)(0x8000 | red | (green << 5) | (blue << 10));
}
uint16_t draw_rgb555(int r, int g, int b);
void draw_rectangle(uint16_t *pixels, int x, int y, int width, int height, uint16_t color);
void draw_text(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
               int max_characters);
void draw_text_clipped(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
                       int max_characters);
/* Pixel-clipped overflow: immediate, continuous tail-to-head looping. */
void draw_text_scrolling(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
                         int width, unsigned elapsed_frames, bool animate);
void draw_text_wrapped(uint16_t *pixels, int x, int y, const char *value, uint16_t color,
                       int line_width, int max_lines);
void draw_placeholder_icon(uint16_t *pixels, int x, int y, int size, PlaceholderIcon icon,
                           uint16_t color);
bool draw_theme_icon_asset(const ThemePackage *package, uint16_t *pixels, int x, int y, int size,
                           int icon);
void draw_theme_icon(const ThemePackage *package, uint16_t *pixels, int x, int y, int size,
                     PlaceholderIcon icon, uint16_t color);
bool draw_theme_component(const ThemePackage *package, uint16_t *pixels, ThemeComponent component,
                          int x, int y, int width, int height);
void draw_theme_selection(const ThemePackage *package, uint16_t *pixels, int x, int y, int width,
                          int height);
void draw_theme_chrome(const ThemePackage *package, uint16_t *pixels, int header_height);
void draw_theme_status(const App *app, uint16_t *pixels);
#endif
