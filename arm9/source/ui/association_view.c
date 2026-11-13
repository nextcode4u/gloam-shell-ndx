#include "association_editor.h"
#include "associations.h"
#include "drawing.h"
#include "themes.h"
#include "ui_strings.h"
#include <stdio.h>

void association_editor_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    const AssociationEditor *editor = &app->association_editor;
    ThemePalette p = themes_palette(app);
    themes_background_draw(&app->themes.active, 0, top);
    draw_theme_chrome(&app->themes.active, top, 22);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_theme_chrome(&app->themes.active, bottom, 22);
    draw_text(top, 10, 8,
              ui_translate(&app->language, "dialog.associations_title", "FILE ASSOCIATIONS"),
              p.accent, 32);
    const char *extension = editor->count ? editor->extensions[editor->selected] : "";
    char label[48];
    snprintf(label, sizeof label, ".%s files", extension);
    draw_text(top, 10, 33, label, p.text, 32);
    const Association *custom = association_user(app, extension);
    char filename[32];
    snprintf(filename, sizeof filename, "file.%s", extension);
    const Association *effective = association_find(app, filename);
    const Association *choices[ASSOCIATION_CAP];
    bool shared = association_choices(app, filename, choices) > 1;
    draw_text(
        top, 10, 57,
        custom && custom->disabled ? ui_translate(&app->language, "dialog.disabled", "Disabled")
        : custom
            ? ui_translate(&app->language, "dialog.personal_default", "Personal global default")
        : shared
            ? ui_translate(&app->language, "dialog.folder_rule", "Folder rules / choose on launch")
            : ui_translate(&app->language, "dialog.supplied_default", "Supplied default"),
        p.accent, 32);
    draw_text_wrapped(top, 10, 80,
                      effective ? effective->application
                      : shared  ? "Matching folders select their preferred app. Other folders open "
                                  "an application chooser."
                                : "No automatic application configured.",
                      p.text, 33, 5);
    draw_text_wrapped(top, 10, 146,
                      "Per-file choices take priority. Content and applications stay in place.",
                      p.muted, 33, 3);
    draw_text(bottom, 10, 7,
              ui_translate(&app->language, "dialog.extension_defaults", "Extension defaults"),
              p.text, 32);
    draw_text(
        bottom, 10, 24,
        ui_translate(&app->language, "dialog.select_back_close", "A select  B back  START close"),
        p.muted, 33);
    const char *options[] = {
        ui_translate(&app->language, "dialog.choose_default", "Choose global default"),
        ui_translate(&app->language, "dialog.restore_rules", "Restore profile rules"),
        ui_translate(&app->language, "dialog.disable_association",
                     "Disable automatic association")};
    int count = editor->option >= 0 ? 3 : editor->count;
    for (int row = 0; row < 7; row++) {
        int index = editor->option >= 0 ? row : editor->scroll + row;
        if (index >= count)
            break;
        int y = 41 + row * 18;
        bool selected = index == (editor->option >= 0 ? editor->option : editor->selected);
        if (selected) {
            draw_theme_selection(&app->themes.active, bottom, 6, y, 244, 18);
        }
        draw_text_clipped(bottom, 14, y + 3,
                          editor->option >= 0 ? options[index] : editor->extensions[index], p.text,
                          32);
    }
    if (!editor->count)
        draw_text(
            bottom, 14, 60,
            ui_translate(&app->language, "dialog.no_profiles", "No extension profiles loaded"),
            p.muted, 32);
    draw_text_wrapped(bottom, 10, 169, editor->status, p.muted, 33, 2);
}
