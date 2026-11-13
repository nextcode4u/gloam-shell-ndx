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
    if (package->minimal_chrome)
        return;
    draw_theme_component(package, pixels, THEME_HEADER, 4, 2, 248, header_height);
    draw_theme_component(package, pixels, THEME_FOOTER, 4, 176, 248, 16);
}

/* Status artwork scales as a whole sprite, never as a nine-slice panel. */
static bool status_sprite(const ThemePackage *package, uint16_t *pixels, ThemeComponent component,
                          int x)
{
    const uint16_t *image = themes_component(package, component);
    if (!image)
        return false;
    for (int y = 0; y < 16; y++)
        for (int xx = 0; xx < 16; xx++) {
            uint16_t color = image[y * 2 * 32 + xx * 2];
            if (color & 0x8000)
                pixels[(y + 4) * 256 + x + xx] = color;
        }
    return true;
}
void draw_theme_status(const App *app, uint16_t *pixels)
{
    const ThemePackage *package = &app->themes.active;
    ThemeComponent battery = themes_battery_component(app->battery_level, app->battery_charging);
    if (!status_sprite(package, pixels, battery, 154)) {
        uint16_t color = battery == THEME_BATTERY_EMPTY || battery == THEME_BATTERY_LOW
                             ? package->palette.accent
                             : package->palette.muted;
        draw_rectangle(pixels, 154, 7, 14, 9, color);
        draw_rectangle(pixels, 156, 9, 10, 5, package->palette.bg);
        draw_rectangle(pixels, 168, 10, 2, 3, color);
        if (battery == THEME_BATTERY_UNKNOWN)
            draw_text(pixels, 158, 6, "?", color, 1);
        else if (app->battery_charging)
            draw_text(pixels, 158, 6, "+", color, 1);
        else if (app->battery_level > 0)
            draw_rectangle(pixels, 156, 9, (app->battery_level * 10 + 14) / 15, 5, color);
    }
    /* Replace the idle wireless slot with the update notice, preserving header bounds. */
    if (app->updates.available) {
        if (!status_sprite(package, pixels, THEME_UPDATE_AVAILABLE, 176)) {
            uint16_t ink = package->palette.accent;
            draw_rectangle(pixels, 182, 8, 3, 9, ink);
            for (int row = 0; row < 5; row++)
                draw_rectangle(pixels, 183 - row, 6 + row, 1 + row * 2, 1, ink);
            draw_rectangle(pixels, 178, 18, 11, 1, ink);
        }
        return;
    }
    if (app->updates.connected) {
        draw_text(pixels, 176, 6, "W", package->palette.accent, 1);
        return;
    }
    if (!status_sprite(package, pixels, THEME_WIRELESS_IDLE, 176)) {
        draw_text(pixels, 176, 6, "W", package->palette.muted, 1);
        draw_rectangle(pixels, 184, 14, 6, 1, package->palette.muted);
    }
}
