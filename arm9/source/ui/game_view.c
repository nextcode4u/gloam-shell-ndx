#include "drawing.h"
#include "game_options.h"
#include "themes.h"
#include "ui_strings.h"
#include <stdio.h>
#include <string.h>
void game_editor_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    const GameEditor *editor = &app->game_editor;
    ThemePalette p = themes_palette(app);
    themes_background_draw(&app->themes.active, 0, top);
    draw_theme_chrome(&app->themes.active, top, 22);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_theme_chrome(&app->themes.active, bottom, 22);
    draw_heading(&app->themes.active, top, 10, 8,
                 editor->defaults
                     ? ui_translate(&app->language, "dialog.game_defaults_title", "GAME DEFAULTS")
                     : ui_translate(&app->language, "dialog.title_options_title", "TITLE OPTIONS"),
                 themes_header_color(&app->themes.active, p.accent), 224);
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
        "QUIT_PATH points to BOOT.NDS.",
        "Language for nds-bootstrap's in-game menu, independent of the game's language. "
        "Keep INI preserves a manually configured backend value or its default.",
        "Hold this combination to swap screens through nds-bootstrap. Keep INI preserves "
        "the configured combination or the backend default L+R+Up+X.",
        "Sidecar .txt uses Game.txt beside Game.nds in the backend menu. Missing or empty "
        "manuals clear the previous title's path. Off clears it; Keep INI leaves it unchanged."};
    static const char *help_keys[] = {
        "dialog.game_mode.help",        "dialog.game_cpu.help",
        "dialog.game_vram.help",        "dialog.game_dma.help",
        "dialog.game_async.help",       "dialog.game_language.help",
        "dialog.game_slot.help",        "dialog.game_size.help",
        "dialog.game_hotkey.help",      "dialog.game_overlay_language.help",
        "dialog.game_screen_swap.help", "dialog.game_manual.help"};
    draw_text_wrapped(
        top, 10, 77,
        ui_translate(&app->language, help_keys[editor->selected], help[editor->selected]), p.text,
        33, 7);
    draw_text(top, 10, 174,
              ui_translate(&app->language, "dialog.backend_experimental",
                           "Backend handoff: experimental"),
              p.muted, 33);
    draw_heading(&app->themes.active, bottom, 10, 7,
                 editor->defaults
                     ? ui_translate(&app->language, "dialog.game_defaults", "Game defaults")
                     : ui_translate(&app->language, "dialog.title_options", "Title options"),
                 themes_header_color(&app->themes.active, p.text), 224);
    draw_text(bottom, 10, 24,
              ui_translate(&app->language, "dialog.change_reset", "< > change  Y reset"), p.muted,
              33);
    static const char *label_keys[] = {
        "dialog.game_mode_label",        "dialog.game_cpu_label",
        "dialog.game_vram_label",        "dialog.game_dma_label",
        "dialog.game_async_label",       "dialog.game_language_label",
        "dialog.game_slot_label",        "dialog.game_size_label",
        "dialog.game_hotkey_label",      "dialog.game_overlay_language_label",
        "dialog.game_screen_swap_label", "dialog.game_manual_label"};
    for (int row = 0; row < 7; row++) {
        int option = editor->scroll + row;
        if (option >= GAME_OPTION_COUNT)
            break;
        int y = 41 + row * 18;
        if (option == editor->selected) {
            draw_theme_selection(&app->themes.active, bottom, 6, y, 244, 18);
        }
        draw_text(bottom, 14, y + 3,
                  ui_translate(&app->language, label_keys[option], game_option_label(option)),
                  option == editor->selected ? themes_selection_text(&app->themes.active) : p.text,
                  16);
        char value[48];
        game_option_value(&editor->options, option, value);
        if (!editor->defaults && editor->options.values[option] == GAME_INHERIT) {
            char effective[48];
            game_option_value(&editor->inherited, option, effective);
            snprintf(value, sizeof value, "Default: %.38s", effective);
        }
        draw_text_clipped(
            bottom, 132, y + 3, value,
            option == editor->selected ? themes_selection_text(&app->themes.active) : p.accent, 16);
    }
    draw_text_clipped(bottom, 10, 170, editor->status, p.muted, 33);
    draw_text(bottom, 10, 182,
              ui_translate(&app->language, "dialog.save_back", "B / START save and back"),
              themes_footer_color(&app->themes.active, p.muted), 33);
}
