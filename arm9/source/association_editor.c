/* Edit personal extension defaults without rewriting distributed profiles. */
#include "association_editor.h"
#include "associations.h"
#include "browser.h"
#include "icons.h"
#include "paths.h"
#include "picker.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int compare(const void *left, const void *right)
{
    return strcmp(left, right);
}
static void add_extensions(AssociationEditor *editor, const char *extensions)
{
    char list[128];
    path_copy(list, extensions, sizeof list);
    char *context;
    for (char *token = strtok_r(list, ", ", &context); token;
         token = strtok_r(NULL, ", ", &context)) {
        if (*token == '.')
            token++;
        if (!association_extension_valid(token))
            continue;
        for (char *p = token; *p; p++)
            *p = (char)tolower((unsigned char)*p);
        bool exists = false;
        for (int i = 0; i < editor->count; i++)
            exists |= !strcmp(editor->extensions[i], token);
        if (!exists && editor->count < EDITOR_EXTENSION_CAP)
            path_copy(editor->extensions[editor->count++], token, EXTENSION_CAP);
    }
}
void association_editor_open(App *app)
{
    AssociationEditor *editor = &app->association_editor;
    memset(editor, 0, sizeof *editor);
    editor->open = true;
    editor->option = -1;
    for (int i = 0; i < app->association_count; i++)
        add_extensions(editor, app->associations[i].extensions);
    for (int i = 0; i < app->user_association_count; i++)
        add_extensions(editor, app->user_associations[i].extensions);
    const Entry *selected = app_selected(app);
    if (selected && selected->kind == ITEM_FILE)
        add_extensions(editor, path_extension(selected->path));
    qsort(editor->extensions, editor->count, EXTENSION_CAP, compare);
    if (selected && selected->kind == ITEM_FILE)
        for (int i = 0; i < editor->count; i++)
            if (!strcasecmp(editor->extensions[i], path_extension(selected->path)))
                editor->selected = i;
    editor->scroll = editor->selected >= 7 ? editor->selected - 6 : 0;
}
void association_editor_move(App *app, int direction)
{
    AssociationEditor *editor = &app->association_editor;
    int count = editor->option >= 0 ? 3 : editor->count;
    if (!count)
        return;
    int *selection = editor->option >= 0 ? &editor->option : &editor->selected;
    *selection = (*selection + direction + count) % count;
    if (editor->option < 0) {
        if (*selection < editor->scroll)
            editor->scroll = *selection;
        if (*selection >= editor->scroll + 7)
            editor->scroll = *selection - 6;
    }
    editor->status[0] = 0;
}
void association_editor_back(App *app)
{
    if (app->association_editor.option >= 0)
        app->association_editor.option = -1;
    else
        app->association_editor.open = false;
}
void association_editor_activate(App *app)
{
    AssociationEditor *editor = &app->association_editor;
    if (!editor->count)
        return;
    if (editor->option < 0) {
        editor->option = 0;
        return;
    }
    const char *extension = editor->extensions[editor->selected];
    if (editor->option == 0) {
        if (!picker_open_extension(app, extension))
            path_copy(editor->status, "Picker unavailable; check SD access", sizeof editor->status);
        return;
    }
    bool saved = editor->option == 1 ? association_clear_default(app, extension)
                                     : association_disable(app, extension);
    path_copy(editor->status,
              saved ? (editor->option == 1 ? "Supplied default restored"
                                           : "Automatic association disabled")
                    : "Save failed; check SD access",
              sizeof editor->status);
    if (saved) {
        icons_reset(app);
        browser_refresh(app);
    }
}
void association_editor_touch(App *app, int x, int y)
{
    AssociationEditor *editor = &app->association_editor;
    if (x < 6 || x >= 250 || y < 41 || y >= 167)
        return;
    int index = (y - 41) / 18;
    if (editor->option >= 0) {
        if (index >= 3)
            return;
        if (index == editor->option)
            association_editor_activate(app);
        else
            editor->option = index;
    } else {
        index += editor->scroll;
        if (index >= editor->count)
            return;
        if (index == editor->selected)
            association_editor_activate(app);
        else
            association_editor_move(app, index - editor->selected);
    }
}
