#include "association_editor.h"
#include "associations.h"
#include "drawing.h"
#include "themes.h"
#include <stdio.h>

void association_editor_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    const AssociationEditor *editor = &app->association_editor;
    ThemePalette p = themes_palette(app);
    themes_background_draw(&app->themes.active, 0, top);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_text(top, 10, 8, "FILE ASSOCIATIONS", p.accent, 32);
    const char *extension = editor->count ? editor->extensions[editor->selected] : "";
    char label[48];
    snprintf(label, sizeof label, ".%s files", extension);
    draw_text(top, 10, 33, label, p.text, 32);
    const Association *custom = association_user(app, extension);
    char filename[32];
    snprintf(filename, sizeof filename, "file.%s", extension);
    const Association *effective = association_find(app, filename);
    draw_text(top, 10, 57,
              custom && custom->disabled ? "Disabled"
              : custom                   ? "Personal default"
                                         : "Supplied default",
              p.accent, 32);
    draw_text_wrapped(top, 10, 80,
                      effective ? effective->application : "No automatic application configured.",
                      p.text, 33, 5);
    draw_text_wrapped(top, 10, 146,
                      "Per-file choices take priority. Content and applications stay in place.",
                      p.muted, 33, 3);
    draw_text(bottom, 10, 7, "Extension defaults", p.text, 32);
    draw_text(bottom, 10, 24, "A select  B back  START close", p.muted, 33);
    static const char *options[] = {"Choose application", "Restore supplied default",
                                    "Disable automatic association"};
    int count = editor->option >= 0 ? 3 : editor->count;
    for (int row = 0; row < 7; row++) {
        int index = editor->option >= 0 ? row : editor->scroll + row;
        if (index >= count)
            break;
        int y = 41 + row * 18;
        bool selected = index == (editor->option >= 0 ? editor->option : editor->selected);
        if (selected) {
            draw_rectangle(bottom, 6, y, 244, 18, p.surface);
            draw_rectangle(bottom, 6, y, 2, 18, p.accent);
        }
        draw_text_clipped(bottom, 14, y + 3,
                          editor->option >= 0 ? options[index] : editor->extensions[index], p.text,
                          32);
    }
    if (!editor->count)
        draw_text(bottom, 14, 60, "No extension profiles loaded", p.muted, 32);
    draw_text_wrapped(bottom, 10, 169, editor->status, p.muted, 33, 2);
}
