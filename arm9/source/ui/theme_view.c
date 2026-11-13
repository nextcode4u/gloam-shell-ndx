#include "ui_strings.h"
/* Preview uses the candidate snapshot; no asset reads or preference writes. */
#include "drawing.h"
#include "themes.h"

void themes_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    const ThemeState *state = &app->themes;
    const ThemePackage *package = state->ready ? &state->candidate : &state->active;
    ThemePalette palette = package->palette;
    themes_background_draw(package, 0, top);
    draw_theme_chrome(package, top, 22);
    themes_background_draw(package, 1, bottom);
    draw_theme_chrome(package, bottom, 22);
    draw_text(top, 10, 8, ui_translate(&app->language, "dialog.theme_preview", "THEME PREVIEW"),
              palette.accent, 30);
    if (package->top_layout == 3) {
        themes_background_scene_draw(package, top);
        draw_theme_component(package, top, THEME_CARD, 14, 44, 48, 48);
        draw_theme_icon(package, top, 18, 48, 40, PLACEHOLDER_FOLDER, palette.accent);
        draw_rectangle(top, 6, 119, 244, 32, palette.bg);
        draw_text_clipped(top, 10, 121, package->name, palette.text, 33);
        draw_text(top, 10, 137, "Aa Bb Cc 0123 !? / .", palette.muted, 32);
    } else {
        draw_text_clipped(top, 10, 32, package->name, palette.text, 33);
        draw_theme_selection(package, top, 10, 56, 236, 22);
        draw_theme_icon(package, top, 15, 59, 16, PLACEHOLDER_FOLDER, palette.accent);
        draw_text(top, 37, 61, "Nintendo DS / Sample", themes_selection_text(package), 27);
        draw_text(top, 10, 93, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", palette.text, 32);
        draw_text(top, 10, 111, "abcdefghijklmnopqrstuvwxyz", palette.muted, 32);
        draw_text(top, 10, 129, "0123456789 !? / . ( )", palette.text, 32);
    }
    if (!draw_theme_component(package, top, THEME_TAB_SELECTED, 10, 153, 82, 22))
        draw_rectangle(top, 10, 153, 82, 22, palette.accent);
    draw_text(top, 21, 158, ui_translate(&app->language, "dialog.selected", "Selected"),
              themes_tab_text(package), 11);
    draw_text(top, 105, 158,
              package->external_font
                  ? ui_translate(&app->language, "dialog.theme_font", "Theme font")
                  : ui_translate(&app->language, "dialog.builtin_font", "Built-in font"),
              palette.muted, 20);
    draw_text(bottom, 10, 7, ui_translate(&app->language, "dialog.choose_theme", "Choose theme"),
              palette.text, 30);
    draw_text(bottom, 10, 24,
              ui_translate(&app->language, "dialog.preview_hint", "Up/down preview; A applies"),
              palette.muted, 33);
    for (int row = 0; row < 7; row++) {
        int index = state->scroll + row;
        if (index >= state->count)
            break;
        int y = 41 + row * 18;
        if (index == state->selected) {
            draw_theme_selection(package, bottom, 6, y, 244, 18);
        }
        draw_text_clipped(bottom, 14, y + 3,
                          index ? state->ids[index]
                                : ui_translate(&app->language, "dialog.builtin", "Built-in"),
                          palette.text, 31);
    }
    draw_text_wrapped(bottom, 10, 168, state->status, palette.muted, 33, 2);
}
