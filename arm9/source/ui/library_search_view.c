#include "drawing.h"
#include "library_search.h"
#include "themes.h"
#include <stdio.h>
#include <string.h>

static const char keyboard[] = "qwertyuiopasdfghjkl-zxcvbnm_./0123456789";
static void button(const App *app, uint16_t *screen, int x, int y, int width, const char *label,
                   bool selected)
{
    const ThemePackage *theme = &app->themes.active;
    ThemePalette p = themes_palette(app);
    if (selected)
        draw_theme_selection(theme, screen, x, y, width, 20);
    else if (!draw_theme_component(theme, screen, THEME_ROW, x, y, width, 20))
        draw_rectangle(screen, x, y, width, 20, p.surface);
    draw_text_scrolling(screen, x + 4, y + 4, label,
                        selected ? themes_selection_text(theme) : p.text, width - 8,
                        app->search->idle, selected && !app->reduced_motion);
}
void library_search_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    const LibrarySearch *s = app->search;
    const ThemePackage *theme = &app->themes.active;
    ThemePalette p = themes_palette(app);
    themes_background_draw(theme, 0, top);
    themes_background_draw(theme, 1, bottom);
    draw_theme_chrome(theme, top, 22);
    draw_theme_chrome(theme, bottom, 22);
    draw_theme_component(theme, top, THEME_PANEL, 5, 26, 246, 142);
    draw_theme_component(theme, bottom, THEME_PANEL, 5, 25, 246, 143);
    draw_heading(theme, top, 10, 7,
                 ui_translate(&app->language, "dialog.search_library", "Search library"),
                 themes_header_color(theme, p.text), 232);
    draw_text(top, 10, 31, s->built, p.muted, 33);
    draw_text_wrapped(top, 10, 51, s->status, p.text, 33, 3);
    draw_text(top, 10, 96, s->all ? "All configured libraries" : "This tab's library", p.accent,
              33);
    draw_text_scrolling(top, 10, 113, s->all ? "Games, Apps, Media and custom folders" : s->scope,
                        p.muted, 236, s->idle, !app->reduced_motion);
    if (s->selected < s->count) {
        const char *path = s->results[s->selected], *name = strrchr(path, '/');
        draw_text_scrolling(top, 10, 137, name ? name + 1 : path, p.text, 236, s->idle,
                            !app->reduced_motion);
        draw_text_scrolling(top, 10, 154, path, p.muted, 236, s->idle, !app->reduced_motion);
    }
    char info[64];
    snprintf(info, sizeof info, "Page %u  %u shown%s", s->page + 1, s->count,
             s->pending ? " / searching"
             : s->more  ? " / more"
                        : "");
    draw_text(top, 10, 176, info, themes_footer_color(theme, p.muted), 33);
    draw_heading(theme, bottom, 10, 7, s->editing ? "Search words" : "Results",
                 themes_header_color(theme, p.text), 164);
    draw_text(bottom, 181, 8, s->all ? "[All]" : "[Tab]", themes_header_color(theme, p.accent), 9);
    draw_text_scrolling(bottom, 10, 29, *s->query ? s->query : "(all indexed files)", p.accent, 236,
                        s->idle, !app->reduced_motion);
    if (s->editing) {
        for (unsigned i = 0; i < 40; i++) {
            char letter[2] = {keyboard[i], 0};
            button(app, bottom, 8 + (i % 10) * 24, 53 + (i / 10) * 22, 23, letter, s->cursor == i);
        }
        button(app, bottom, 8, 145, 78, "Space", s->cursor == 40);
        button(app, bottom, 88, 145, 78, "Erase", s->cursor == 41);
        button(app, bottom, 168, 145, 80, "Search", s->cursor == 42);
    } else {
        for (unsigned row = 0; row < 6 && row + s->scroll < s->count; row++) {
            unsigned index = row + s->scroll;
            int y = 48 + row * 18;
            bool selected = index == s->selected;
            if (selected)
                draw_theme_selection(theme, bottom, 8, y, 240, 18);
            else
                draw_theme_component(theme, bottom, THEME_ROW, 8, y, 240, 18);
            const char *path = s->results[index], *name = strrchr(path, '/');
            draw_text_scrolling(bottom, 12, y + 3, name ? name + 1 : path,
                                selected ? themes_selection_text(theme) : p.text, 232, s->idle,
                                selected && !app->reduced_motion);
        }
        draw_text(bottom, 12, 157, "< L previous          R next >", p.muted, 33);
    }
    draw_text(bottom, 10, 176,
              ui_translate(&app->language, "dialog.search_hint", "B back  X edit  SELECT scope"),
              themes_footer_color(theme, p.muted), 33);
}
