/* Preserve corners; theme surfaces can stretch or repeat their material center. */
#include "drawing.h"
#include "theme_motion.h"
#include "themes.h"

static unsigned source_position(int position, int length, unsigned border, bool tiled)
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
    unsigned middle = 32 - 2 * border;
    if (tiled)
        return border + (unsigned)(position - edge) % middle;
    return border + (unsigned)(position - edge) * middle / (length - 2 * edge);
}
bool draw_theme_component(const ThemePackage *package, uint16_t *pixels, ThemeComponent component,
                          int x, int y, int width, int height)
{
    const uint16_t *image = themes_component(package, component);
    const uint8_t *coverage = themes_component_alpha(package, component);
    if (!image || width <= 0 || height <= 0 || width > 512 || height > 384 ||
        package->component_border > 12 || x < -512 || x > 512 || y < -384 || y > 384)
        return false;
    const uint16_t *material_colors;
    const uint8_t *material = themes_component_material(package, component, &material_colors);
    int left = x < 0 ? -x : 0, top = y < 0 ? -y : 0;
    int right = x + width > 256 ? 256 - x : width;
    int bottom = y + height > 192 ? 192 - y : height;
    unsigned columns[256];
    /* Small control glyphs and narrow scroll tracks retain their usual scaling. */
    bool tiled = package->component_tiled && component <= THEME_ARTWORK_FRAME;
    for (int column = left; column < right; column++)
        columns[x + column] = source_position(column, width, package->component_border, tiled);
    for (int row = top; row < bottom; row++) {
        unsigned source = source_position(row, height, package->component_border, tiled) * 32;
        for (int column = left; column < right; column++) {
            unsigned index = source + columns[x + column];
            uint16_t color = image[index];
            if (color & 0x8000) {
                unsigned screen_index = (y + row) * 256 + x + column;
                uint16_t *destination = &pixels[screen_index];
                /* Only the plain fill changes. Trim, symbols, alpha and holes
                 * retain their authored pixels. The field never wraps/repeats. */
                if (material && color == material_colors[8]) {
                    unsigned shade = (material[screen_index / 2] >> ((screen_index % 2) * 4)) & 15;
                    color = material_colors[shade];
                }
                unsigned alpha = coverage ? (coverage[index / 4] >> ((index % 4) * 2)) & 3 : 3;
                *destination = alpha == 3 ? color : draw_blend_coverage(color, *destination, alpha);
            }
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

/* Controls scale as whole sprites; narrow tracks use slices instead. */
bool draw_theme_sprite(const ThemePackage *package, uint16_t *pixels, ThemeComponent component,
                       int x, int y, int width, int height)
{
    const uint16_t *image = themes_component(package, component);
    const uint8_t *coverage = themes_component_alpha(package, component);
    if (!image || width < 1 || height < 1 || width > 256 || height > 192 || x < -256 || x > 256 ||
        y < -192 || y > 192)
        return false;
    for (int yy = 0; yy < height; yy++) {
        if (y + yy < 0 || y + yy >= 192)
            continue;
        for (int xx = 0; xx < width; xx++) {
            if (x + xx < 0 || x + xx >= 256)
                continue;
            unsigned index = (yy * 32 / height) * 32 + xx * 32 / width;
            uint16_t color = image[index];
            if (color & 0x8000) {
                uint16_t *destination = &pixels[(y + yy) * 256 + x + xx];
                unsigned alpha = coverage ? (coverage[index / 4] >> ((index % 4) * 2)) & 3 : 3;
                *destination = alpha == 3 ? color : draw_blend_coverage(color, *destination, alpha);
            }
        }
    }
    return true;
}
void draw_theme_arrow(const ThemePackage *package, uint16_t *pixels, int x, int y,
                      ThemeComponent direction, uint16_t color)
{
    if (direction < THEME_ARROW_LEFT || direction > THEME_ARROW_DOWN)
        return;
    if (draw_theme_sprite(package, pixels, direction, x, y, 8, 10))
        return;
    static const char *glyphs[] = {"<", ">", "^", "v"};
    draw_text(pixels, x, y, glyphs[direction - THEME_ARROW_LEFT], color, 1);
}
void draw_theme_scrollbar(const ThemePackage *package, uint16_t *pixels, int x, int y, int height,
                          int thumb_y, int thumb_height)
{
    if (!draw_theme_component(package, pixels, THEME_SCROLL_TRACK, x, y, 2, height))
        draw_rectangle(pixels, x, y, 2, height, package->palette.line);
    if (!draw_theme_component(package, pixels, THEME_SCROLL_THUMB, x, thumb_y, 2, thumb_height))
        draw_rectangle(pixels, x, thumb_y, 2, thumb_height, package->palette.accent);
}
void draw_theme_focus(const App *app, uint16_t *pixels, int x, int y, int width, int height,
                      unsigned elapsed)
{
    if (!app->themes.active.focus_feedback || app->reduced_motion || elapsed >= 12 || width < 8 ||
        height < 8)
        return;
    unsigned alpha = elapsed < 4 ? 2 : 1;
    /* One short gleam on the margin, without moving a border across adjacent text. */
    for (int yy = y + 3; yy < y + height - 3; yy++)
        if (yy >= 0 && yy < 192 && x + 2 >= 0 && x + 2 < 256)
            pixels[yy * 256 + x + 2] = draw_blend_coverage(app->themes.active.palette.accent,
                                                           pixels[yy * 256 + x + 2], alpha);
}
void draw_theme_status(const App *app, uint16_t *pixels)
{
    const ThemePackage *package = &app->themes.active;
    ThemeComponent battery = themes_battery_component(app->battery_level, app->battery_charging);
    if (!draw_theme_sprite(package, pixels, battery, 154, 4, 16, 16)) {
        uint16_t color = battery == THEME_BATTERY_EMPTY || battery == THEME_BATTERY_LOW
                             ? package->palette.accent
                             : package->palette.muted;
        draw_rectangle(pixels, 154, 7, 14, 9, color);
        draw_rectangle(pixels, 156, 9, 10, 5, package->palette.bg);
        draw_rectangle(pixels, 168, 10, 2, 3, color);
        if (battery == THEME_BATTERY_UNKNOWN)
            draw_text(pixels, 158, 6, "?", themes_header_color(package, color), 1);
        else if (app->battery_charging)
            draw_text(pixels, 158, 6, "+", themes_header_color(package, color), 1);
        else if (app->battery_level > 0)
            draw_rectangle(pixels, 156, 9, (app->battery_level * 10 + 14) / 15, 5, color);
    }
    /* Replace the idle wireless slot with the update notice, preserving header bounds. */
    if (app->updates.available) {
        if (!draw_theme_sprite(package, pixels, THEME_UPDATE_AVAILABLE, 176, 4, 16, 16)) {
            uint16_t ink = package->palette.accent;
            draw_rectangle(pixels, 182, 8, 3, 9, ink);
            for (int row = 0; row < 5; row++)
                draw_rectangle(pixels, 183 - row, 6 + row, 1 + row * 2, 1, ink);
            draw_rectangle(pixels, 178, 18, 11, 1, ink);
        }
        return;
    }
    if (app->updates.connected) {
        draw_text(pixels, 176, 6, "W", themes_header_color(package, package->palette.accent), 1);
        return;
    }
    if (!draw_theme_sprite(package, pixels, THEME_WIRELESS_IDLE, 176, 4, 16, 16)) {
        draw_text(pixels, 176, 6, "W", themes_header_color(package, package->palette.muted), 1);
        draw_rectangle(pixels, 184, 14, 6, 1, package->palette.muted);
    }
}

void draw_theme_ambient(const App *app, uint16_t *pixels)
{
    const uint8_t *coverage;
    const uint16_t *frame = themes_motion_frame(app, &coverage);
    if (!frame)
        return;
    /* This reserved header mark never crosses titles, covers, tabs or status icons. */
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            unsigned index = y * 64 + x * 2;
            unsigned alpha = (coverage[index / 4] >> ((index % 4) * 2)) & 3;
            if (!alpha)
                continue;
            uint16_t *pixel = &pixels[(y + 4) * 256 + x + 136];
            *pixel = alpha == 3 ? frame[index] : draw_blend_coverage(frame[index], *pixel, alpha);
        }
}
