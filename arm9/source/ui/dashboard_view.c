#include "dashboard.h"
#include "drawing.h"
#include "themes.h"
#include "ui_strings.h"
#include <stdio.h>

void dashboard_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    ThemePalette p = themes_palette(app);
    themes_background_draw(&app->themes.active, 0, top);
    draw_theme_chrome(&app->themes.active, top, 22);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_theme_chrome(&app->themes.active, bottom, 22);
    draw_heading(&app->themes.active, top, 10, 8,
                 ui_translate(&app->language, "dialog.dashboard", "PERSONAL DASHBOARD"), p.accent,
                 224);
    draw_text_wrapped(top, 10, 37,
                      "Choose which sections appear on Home and their order. Press START to "
                      "open frontend settings.",
                      p.text, 33, 5);
    draw_text_wrapped(top, 10, 108,
                      "Recent entries record launch requests, not confirmed gameplay or playtime. "
                      "Hiding Recent stops recording new requests.",
                      p.muted, 33, 6);
    draw_heading(&app->themes.active, bottom, 10, 7,
                 ui_translate(&app->language, "dialog.home_sections", "Home sections"), p.text,
                 224);
    draw_text(bottom, 10, 24,
              ui_translate(&app->language, "dialog.reorder_hint", "< > reorder  A show/hide"),
              p.muted, 33);
    const char *names[] = {
        ui_translate(&app->language, "dialog.browse_links", "Browse links"),
        ui_translate(&app->language, "dialog.folder_tabs", "Folder tabs"),
        ui_translate(&app->language, "dialog.pinned_items", "Pinned items"),
        ui_translate(&app->language, "dialog.recent_requests", "Recent requests")};
    for (int row = 0; row < 4; row++) {
        int y = 41 + row * 18;
        DashboardSection section = app->dashboard.config.order[row];
        if (row == app->dashboard.selected) {
            draw_theme_selection(&app->themes.active, bottom, 6, y, 244, 18);
        }
        draw_text(bottom, 14, y + 3, names[section], p.text, 21);
        char count[8] = "All";
        if (app->dashboard.config.limit[section])
            snprintf(count, sizeof count, "%u", app->dashboard.config.limit[section]);
        draw_text(bottom, 180, y + 3, count, p.muted, 3);
        draw_text(bottom, 222, y + 3, app->dashboard.config.enabled[section] ? "On" : "Off",
                  p.accent, 3);
    }
    draw_text(bottom, 10, 116,
              ui_translate(&app->language, "dialog.visible_count", "L/R: visible item count"),
              p.muted, 33);
    draw_text_wrapped(bottom, 10, 135, app->dashboard.status, p.muted, 33, 3);
    draw_text(bottom, 10, 177,
              ui_translate(&app->language, "dialog.clear_recent_hint", "B back  Y clear recent"),
              p.muted, 33);
}
