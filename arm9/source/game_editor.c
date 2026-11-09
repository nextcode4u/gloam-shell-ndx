#include "app.h"
#include "browser.h"
#include "game_options.h"
#include "icons.h"
#include "paths.h"
#include <string.h>

void game_editor_open(App *app, bool defaults)
{
    const Entry *entry = app_selected(app);
    if (!app->storage_ready ||
        (!defaults && (!entry || entry->kind != ITEM_FILE || !icon_is_ds_title(entry->path)))) {
        path_copy(app->message, "Select a DS title with SD access to edit game options.",
                  sizeof app->message);
        app_panel(app, PANEL_MESSAGE);
        return;
    }
    GameEditor *editor = &app->game_editor;
    memset(editor, 0, sizeof *editor);
    editor->defaults = defaults;
    game_options_effective(app->root, "", &editor->inherited);
    if (!defaults)
        path_copy(editor->path, entry->path, PATH_CAP);
    if (defaults)
        game_options_effective(app->root, "", &editor->options);
    else
        game_options_read(app->root, editor->path, &editor->options);
    editor->open = true;
    if (app->panel != PANEL_ACTIONS)
        app->panel = PANEL_NONE;
}
void game_editor_move(App *app, int direction)
{
    GameEditor *editor = &app->game_editor;
    editor->selected = (editor->selected + direction + GAME_OPTION_COUNT) % GAME_OPTION_COUNT;
    if (editor->selected < editor->scroll)
        editor->scroll = editor->selected;
    if (editor->selected >= editor->scroll + 7)
        editor->scroll = editor->selected - 6;
}
bool game_editor_close(App *app)
{
    GameEditor *editor = &app->game_editor;
    if (!game_options_write(app->root, editor->path, &editor->options)) {
        path_copy(editor->status, "Save failed: check SD access", sizeof editor->status);
        return false;
    }
    editor->open = false;
    return true;
}
void game_editor_touch(App *app, int x, int y)
{
    if (x < 6 || x >= 250 || y < 41 || y >= 167)
        return;
    int row = (y - 41) / 18 + app->game_editor.scroll;
    if (row >= GAME_OPTION_COUNT)
        return;
    if (row == app->game_editor.selected)
        game_editor_adjust(app, x < 128 ? -1 : 1);
    else
        app->game_editor.selected = row;
}
