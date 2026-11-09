#include "drawing.h"
#include "game_options.h"
#include "themes.h"
#include <stdio.h>
#include <string.h>
void game_editor_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    const GameEditor *editor = &app->game_editor;
    ThemePalette p = themes_palette(app);
    themes_background_draw(&app->themes.active, 0, top);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_text(top, 10, 8, editor->defaults ? "GAME DEFAULTS" : "TITLE OPTIONS", p.accent, 32);
    const char *name = strrchr(editor->path, '/');
    draw_text_wrapped(top, 10, 30,
                      editor->defaults ? "Applies to nds-bootstrap launches"
                                       : (name ? name + 1 : editor->path),
                      p.text, 33, 3);
    static const char *help[] = {
        "Auto lets the backend decide DS or DSi mode. A DSi-enhanced title and compatible "
        "environment are required for DSi features.",
        "CPU boost requests the DSi's faster CPU through nds-bootstrap. It does not alter direct "
        "emulator launches.",
        "VRAM boost is a backend option. Auto keeps backend defaults; some games require it "
        "disabled.",
        "Card DMA controls backend ROM reads. Disable it when diagnosing loading compatibility.",
        "Asynchronous reads are backend-specific. Auto uses its defaults; disabling may help "
        "compatibility.",
        "Override the language passed to the game. Auto uses the console/backend language.",
        "Slot 0 reuses recognized saves. Slots 1-9 use independent .savN files, or .pubN/.prvN "
        "for DSiWare. Existing saves are not replaced.",
        "Auto uses the DS save database or DSiWare public/private header sizes. Unknown DS games "
        "need an explicit size. Existing saves are never resized.",
        "Opens nds-bootstrap's in-game menu. Return depends on backend/game compatibility; "
        "QUIT_PATH points to BOOT.NDS."};
    draw_text_wrapped(top, 10, 77, help[editor->selected], p.text, 33, 7);
    draw_text(top, 10, 174, "Backend handoff: experimental", p.muted, 33);
    draw_text(bottom, 10, 7, editor->defaults ? "Game defaults" : "Title options", p.text, 32);
    draw_text(bottom, 10, 24, "< > change  Y reset", p.muted, 33);
    for (int row = 0; row < 7; row++) {
        int option = editor->scroll + row;
        if (option >= GAME_OPTION_COUNT)
            break;
        int y = 41 + row * 18;
        if (option == editor->selected) {
            draw_rectangle(bottom, 6, y, 244, 18, p.surface);
            draw_rectangle(bottom, 6, y, 2, 18, p.accent);
        }
        draw_text(bottom, 14, y + 3, game_option_label(option), p.text, 16);
        char value[48];
        game_option_value(&editor->options, option, value);
        if (!editor->defaults && editor->options.values[option] == GAME_INHERIT) {
            char effective[48];
            game_option_value(&editor->inherited, option, effective);
            snprintf(value, sizeof value, "Default: %.38s", effective);
        }
        draw_text_clipped(bottom, 132, y + 3, value, p.accent, 16);
    }
    draw_text_clipped(bottom, 10, 170, editor->status, p.muted, 33);
    draw_text(bottom, 10, 182, "B / START save and back", p.muted, 33);
}
