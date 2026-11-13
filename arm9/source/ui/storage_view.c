#include "drawing.h"
#include "storage_state.h"
#include "themes.h"
void storage_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    ThemePalette p = themes_palette(app);
    themes_background_draw(&app->themes.active, 0, top);
    draw_theme_chrome(&app->themes.active, top, 22);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_theme_chrome(&app->themes.active, bottom, 22);
    draw_text(top, 10, 10, "SD CARD REMOVED", p.accent, 32);
    draw_text_wrapped(top, 10, 42,
                      "Browsing and audio are paused. New launches and preference writes are "
                      "blocked. Your saved configuration is unchanged.",
                      p.text, 33, 7);
    draw_text(bottom, 10, 10, app->storage_card_present ? "Card detected again" : "Card is absent",
              p.accent, 33);
    draw_text_wrapped(bottom, 10, 43,
                      "Insert the SD card, then restart the frontend to mount it safely. Unsaved "
                      "changes in this session cannot be saved here.",
                      p.text, 33, 7);
    draw_text_wrapped(bottom, 10, 145, "START: return to DSi Menu without saving", p.muted, 33, 3);
}
