/* One paged settings view shared by START, touch and the dashboard shortcut. */
#include "drawing.h"
#include "settings.h"
#include "themes.h"
#include "ui_strings.h"
#include <stdio.h>

void settings_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    ThemePalette palette = themes_palette(app);
    const SettingsState *state = &app->settings;
    unsigned elapsed = app->frame_count - state->text_started_frame;
    const SettingsOption *option = settings_option(app, state->section, state->selected);
    if (!option)
        option = settings_option(app, 0, 0);
    themes_background_draw(&app->themes.active, 0, top);
    draw_theme_chrome(&app->themes.active, top, 22);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_theme_chrome(&app->themes.active, bottom, 22);
    draw_text(top, 10, 8,
              app->simple_settings_lock ? ui_translate(&app->language, "dialog.settings_locked",
                                                       "SIMPLE SETTINGS - LOCKED")
              : settings_is_simple(app)
                  ? ui_translate(&app->language, "dialog.settings_simple", "SIMPLE SETTINGS")
                  : ui_translate(&app->language, "dialog.settings_advanced", "ADVANCED SETTINGS"),
              palette.accent, 33);
    draw_rectangle(top, 10, 23, 236, 1, palette.line);
    draw_text_clipped(top, 10, 31, settings_option_text(app, option, false), palette.text, 33);
    char value[96];
    settings_value(app, option, value, sizeof value);
    draw_text_wrapped(top, 10, 51, value, palette.accent, 33, 2);
    draw_text_wrapped(top, 10, 80, settings_option_text(app, option, true), palette.text, 33, 6);
    draw_text_scrolling(
        top, 10, 160,
        app->recovery_mode ? ui_translate(&app->language, "dialog.recovery_appearance",
                                          "Recovery: appearance is session-only")
        : state->status[0] ? state->status
        : app->preferences_dirty
            ? ui_translate(&app->language, "dialog.pending_save", "Changes waiting to save")
            : ui_translate(&app->language, "dialog.saved", "Preferences saved"),
        palette.muted, 231, elapsed, !app->reduced_motion);
    draw_text(top, 10, 179,
              ui_translate(&app->language, "dialog.settings_close", "L/R pages  B / START close"),
              palette.muted, 33);
    draw_text_clipped(bottom, 10, 7, settings_section_name(app, state->section), palette.text, 26);
    char page[24];
    snprintf(page, sizeof page, "%d/%d", state->section + 1, settings_section_count(app));
    draw_text(bottom, 222, 7, page, palette.accent, 4);
    draw_text(bottom, 10, 24, ui_translate(&app->language, "dialog.previous_page", "< L previous"),
              palette.muted, 14);
    draw_text(bottom, 178, 24, ui_translate(&app->language, "dialog.next_page", "R next >"),
              palette.muted, 10);
    draw_rectangle(bottom, 10, 36, 236, 1, palette.line);
    int count = settings_option_count(app, state->section);
    for (int row = 0; row < SETTINGS_VISIBLE_ROWS; row++) {
        int index = state->scroll + row;
        if (index >= count)
            break;
        int y = SETTINGS_LIST_TOP + row * SETTINGS_ROW_HEIGHT;
        bool selected = index == state->selected;
        if (selected) {
            draw_theme_selection(&app->themes.active, bottom, 6, y, 244, SETTINGS_ROW_HEIGHT);
        }
        const SettingsOption *item = settings_option(app, state->section, index);
        draw_text_scrolling(bottom, 14, y + 3, settings_option_text(app, item, false),
                            selected ? themes_selection_text(&app->themes.active) : palette.muted,
                            126, elapsed, selected && !app->reduced_motion);
        settings_value(app, item, value, sizeof value);
        draw_text_scrolling(bottom, 148, y + 3, value,
                            item->adjustable ? palette.accent : palette.muted, 91, elapsed,
                            selected && !app->reduced_motion);
    }
    draw_rectangle(bottom, 10, 176, 236, 1, palette.line);
    draw_text(bottom, 10, 179,
              option->adjustable ? ui_translate(&app->language, "dialog.settings_adjust",
                                                "< > change  A next  B close")
                                 : ui_translate(&app->language, "dialog.settings_select",
                                                "A select  B close  L/R pages"),
              palette.muted, 34);
}
