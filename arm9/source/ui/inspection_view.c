#include "browser.h"
#include "drawing.h"
#include "inspection.h"
#include "themes.h"

static void field(const App *app, uint16_t *screen, int y, const char *label, const char *value)
{
    ThemePalette p = themes_palette(app);
    draw_text(screen, 10, y, label, p.muted, 33);
    draw_text_scrolling(screen, 10, y + 15, *value ? value : "-", p.text, 236,
                        app->frame_count - app->panel_text_started_frame, !app->reduced_motion);
}
void inspection_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    const LaunchExplanation *details = app->inspection;
    ThemePalette p = themes_palette(app);
    const ThemePackage *theme = &app->themes.active;
    themes_background_draw(theme, 0, top);
    themes_background_draw(theme, 1, bottom);
    draw_theme_chrome(theme, top, 22);
    draw_theme_chrome(theme, bottom, 22);
    draw_theme_component(theme, top, THEME_PANEL, 5, 27, 246, 140);
    draw_theme_component(theme, bottom, THEME_PANEL, 5, 27, 246, 140);
    draw_heading(theme, top, 10, 7,
                 ui_translate(&app->language, "dialog.why_handler", "Why this handler?"),
                 themes_header_color(theme, p.text), 234);
    const Entry *entry = app_selected(app);
    draw_text_scrolling(top, 10, 31, entry ? entry->name : "", p.accent, 236,
                        app->frame_count - app->panel_text_started_frame, !app->reduced_motion);
    draw_text_wrapped(top, 10, 53, details->reason, p.text, 33, 3);
    field(app, top, 98, "Matching rule", details->rule);
    field(app, top, 135, "Application", details->application);
    draw_heading(theme, bottom, 10, 7, "Launch and saves", themes_header_color(theme, p.text), 234);
    field(app, bottom, 31, "Backend", details->backend);
    field(app, bottom, 66, "Save / public save", details->save);
    field(app, bottom, 101, "Private save", details->private_save);
    field(app, bottom, 137, "Preflight", details->status);
    draw_text(bottom, 10, 176, "Left/right: path  B: back", themes_footer_color(theme, p.muted),
              33);
}
