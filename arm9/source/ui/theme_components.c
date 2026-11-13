/* Stretch centers and edges while retaining the artwork's corner proportions. */
#include "drawing.h"
#include "themes.h"

static unsigned source_position(int position, int length, unsigned border)
{
    unsigned edge = border;
    if (edge > (unsigned)length / 2)
        edge = (unsigned)length / 2;
    if (edge && position < (int)edge)
        return edge == 1 ? 0 : (unsigned)position * (border - 1) / (edge - 1);
    if (edge && position >= length - (int)edge)
        return edge == 1
                   ? 31
                   : 32 - border + (unsigned)(position - length + edge) * (border - 1) / (edge - 1);
    return border + (unsigned)(position - edge) * (32 - 2 * border) / (length - 2 * edge);
}
bool draw_theme_component(const ThemePackage *package, uint16_t *pixels, ThemeComponent component,
                          int x, int y, int width, int height)
{
    const uint16_t *image = themes_component(package, component);
    if (!image || width <= 0 || height <= 0 || width > 512 || height > 384 ||
        package->component_border > 12 || x < -512 || x > 512 || y < -384 || y > 384)
        return false;
    int left = x < 0 ? -x : 0, top = y < 0 ? -y : 0;
    int right = x + width > 256 ? 256 - x : width;
    int bottom = y + height > 192 ? 192 - y : height;
    unsigned columns[256];
    for (int column = left; column < right; column++)
        columns[x + column] = source_position(column, width, package->component_border);
    for (int row = top; row < bottom; row++) {
        unsigned source = source_position(row, height, package->component_border) * 32;
        for (int column = left; column < right; column++) {
            uint16_t color = image[source + columns[x + column]];
            if (color & 0x8000)
                pixels[(y + row) * 256 + x + column] = color;
        }
    }
    return true;
}
void draw_theme_selection(const ThemePackage *package, uint16_t *pixels, int x, int y, int width,
                          int height)
{
    if (draw_theme_component(package, pixels, THEME_ROW_SELECTED, x, y, width, height))
        return;
    draw_rectangle(pixels, x, y, width, height, package->palette.surface);
    draw_rectangle(pixels, x, y, 2, height, package->palette.accent);
}
void draw_theme_chrome(const ThemePackage *package, uint16_t *pixels, int header_height)
{
    draw_theme_component(package, pixels, THEME_HEADER, 4, 2, 248, header_height);
    draw_theme_component(package, pixels, THEME_FOOTER, 4, 176, 248, 16);
}
