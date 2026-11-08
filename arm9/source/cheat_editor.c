#include "app.h"
#include "browser.h"
#include "cheats.h"
#include "icons.h"
#include "paths.h"
#include <stdlib.h>

int cheats_visible(const CheatState *state, int indices[CHEAT_ENTRY_CAP])
{
    int count = 0;
    for (int i = 0; i < state->count; i++) {
        int group = state->entries[i].group;
        if (group >= 0 && !state->entries[group].expanded)
            continue;
        indices[count++] = i;
    }
    return count;
}
void cheats_open(App *app)
{
    const Entry *entry = app_selected(app);
    if (!app->storage_ready || !entry || entry->kind != ITEM_FILE ||
        !icon_is_ds_title(entry->path)) {
        path_copy(app->message, "Select a DS title with SD access to edit cheats.",
                  sizeof app->message);
        app_panel(app, PANEL_MESSAGE);
        return;
    }
    CheatState *state = calloc(1, sizeof *state);
    if (!state) {
        path_copy(app->message, "Not enough memory for cheat selection", sizeof app->message);
        app_panel(app, PANEL_MESSAGE);
        return;
    }
    cheats_load(state, app->root, entry->path);
    app->cheats = state;
    state->open = true;
    app->panel = PANEL_NONE;
}
void cheats_move(App *app, int direction)
{
    CheatState *state = app->cheats;
    int indices[CHEAT_ENTRY_CAP];
    int count = cheats_visible(state, indices);
    if (!count)
        return;
    state->selected = (state->selected + direction + count) % count;
    if (state->selected < state->scroll)
        state->scroll = state->selected;
    if (state->selected >= state->scroll + 7)
        state->scroll = state->selected - 6;
}
void cheats_activate(App *app)
{
    int indices[CHEAT_ENTRY_CAP];
    int count = cheats_visible(app->cheats, indices);
    if (app->cheats->selected < count)
        cheats_toggle(app->cheats, indices[app->cheats->selected]);
}
bool cheats_close(App *app)
{
    CheatState *state = app->cheats;
    if (!state)
        return true;
    if (state->record && (!app->storage_ready || !cheats_save(state, app->root))) {
        path_copy(state->status, "Save failed: check SD access", sizeof state->status);
        return false;
    }
    cheats_free(state);
    free(state);
    app->cheats = NULL;
    return true;
}
void cheats_touch(App *app, int x, int y)
{
    if (x < 6 || x >= 250 || y < 41 || y >= 167)
        return;
    int indices[CHEAT_ENTRY_CAP];
    int count = cheats_visible(app->cheats, indices);
    int row = app->cheats->scroll + (y - 41) / 18;
    if (row >= count)
        return;
    if (row == app->cheats->selected)
        cheats_activate(app);
    else
        app->cheats->selected = row;
}
