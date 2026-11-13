#ifndef FRONTEND_UI_DRAWING_H
#define FRONTEND_UI_DRAWING_H
#include "file_types.h"
#include "themes.h"
#include <stdbool.h>
#include <stdint.h>
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
void draw_theme_icon(const ThemePackage *package, uint16_t *pixels, int x, int y, int size,
                     PlaceholderIcon icon, uint16_t color);
bool draw_theme_component(const ThemePackage *package, uint16_t *pixels, ThemeComponent component,
                          int x, int y, int width, int height);
void draw_theme_selection(const ThemePackage *package, uint16_t *pixels, int x, int y, int width,
                          int height);
void draw_theme_chrome(const ThemePackage *package, uint16_t *pixels, int header_height);
#endif
