#include "cheats.h"
#include "drawing.h"
#include "themes.h"
#include "ui_strings.h"
#include <stdio.h>
void cheats_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    const CheatState *state = app->cheats;
    ThemePalette p = themes_palette(app);
    themes_background_draw(&app->themes.active, 0, top);
    draw_theme_chrome(&app->themes.active, top, 22);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_theme_chrome(&app->themes.active, bottom, 22);
    draw_text(top, 10, 8, ui_translate(&app->language, "dialog.cheats_title", "TITLE CHEATS"),
              p.accent, 32);
    int indices[CHEAT_ENTRY_CAP], count = cheats_visible(state, indices);
    if (count && state->selected < count) {
        const CheatEntry *entry = &state->entries[indices[state->selected]];
        draw_text_wrapped(top, 10, 32, entry->name, p.text, 33, 3);
        draw_text_wrapped(top, 10, 72, entry->note, p.text, 33, 6);
        draw_text(
            top, 10, 151,
            entry->exclusive
                ? ui_translate(&app->language, "dialog.choose_one", "Choose one in this group")
                : ui_translate(&app->language, "dialog.toggle_group", "A: toggle / open group"),
            p.accent, 33);
    } else
        draw_text_wrapped(top, 10, 38, state->status, p.text, 33, 7);
    draw_text(
        top, 10, 175,
        ui_translate(&app->language, "dialog.cheats_next_launch", "Codes apply on next launch"),
        p.muted, 33);
    int enabled = 0;
    for (int i = 0; i < state->count; i++)
        if (state->entries[i].selected && !state->entries[i].folder)
            enabled++;
    char title[48];
    snprintf(title, sizeof title, "Cheats: %d enabled", enabled);
    draw_text(bottom, 10, 7, title, p.text, 33);
    draw_text(bottom, 10, 24,
              ui_translate(&app->language, "dialog.cheats_hint", "A toggle  Y disable all"),
              p.muted, 33);
    for (int row = 0; row < 7 && row + state->scroll < count; row++) {
        int visible = row + state->scroll, y = 41 + row * 18;
        const CheatEntry *entry = &state->entries[indices[visible]];
        if (visible == state->selected) {
            draw_theme_selection(&app->themes.active, bottom, 6, y, 244, 18);
        }
        draw_text(bottom, 14, y + 3,
                  entry->folder ? (entry->expanded ? "-" : "+") : (entry->selected ? "[x]" : "[ ]"),
                  p.accent, 3);
        draw_text_clipped(bottom, 40, y + 3, entry->name, p.text, 29);
    }
    draw_text_clipped(bottom, 10, 170,
                      state->record ? state->status
                                    : ui_translate(&app->language, "dialog.no_cheats",
                                                   "No matching cheats available"),
                      p.muted, 33);
    draw_text(bottom, 10, 182,
              ui_translate(&app->language, "dialog.save_back", "B / START save and back"), p.muted,
              33);
}
