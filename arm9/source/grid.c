/* Shared directional navigation and touch geometry for the browser's grid. */
#include "grid.h"
#include "app.h"
#include "browser.h"
#include "layout.h"
#include "text.h"

void browser_direction(App *app, int horizontal, int vertical)
{
    if (!app->entry_count)
        return;
    if (app->browser_style == BROWSER_SHELF) {
        app_move(app, horizontal ? horizontal : vertical * 3);
        return;
    }
    if (!layout_is_grid(app)) {
        if (horizontal)
            app_page(app, horizontal);
        else if (vertical)
            app_move(app, vertical);
        return;
    }
    if (horizontal) {
        app_move(app, horizontal);
        return;
    }
    int selected = app->locations[app->category].selected;
    int rows = (app->entry_count + layout_grid_columns(app) - 1) / layout_grid_columns(app);
    int row = (selected / layout_grid_columns(app) + vertical + rows) % rows;
    int target = row * layout_grid_columns(app) + selected % layout_grid_columns(app);
    if (target >= app->entry_count)
        target = app->entry_count - 1;
    if (target != selected)
        app_move(app, target - selected);
}
int grid_touch_index(const App *app, int x, int y)
{
    if (app->browser_style == BROWSER_SHELF) {
        int top = layout_shelf_top(app);
        if (x < 8 || x >= 248 || y < top || y >= top + UI_SHELF_CARD_HEIGHT + 6)
            return -1;
        /* Hit-test the same moving row that is drawn, including partial edges. */
        int shifted = x - 8 - app->shelf_offset_fixed / 256;
        if (shifted < 0 || shifted >= 240)
            return -1;
        int index = app->locations[app->category].selected - 1 + shifted / 80;
        return index >= 0 && index < app->entry_count ? index : -1;
    }
    int top = layout_list_top(app->category);
    if (!layout_is_grid(app) || x < 6 || x >= 250 || y < top || y >= layout_list_bottom(app))
        return -1;
    if ((x - 6) / layout_grid_cell_width(app) >= layout_grid_columns(app))
        return -1;
    int index = app->locations[app->category].scroll +
                ((y - top) / layout_row_height(app)) * layout_grid_columns(app) +
                (x - 6) / layout_grid_cell_width(app);
    return index < app->entry_count ? index : -1;
}

static uint32_t entry_initial(const App *app, int index)
{
    char name[256];
    browser_display_name(app, &app->entries[index], name);
    const char *cursor = name;
    uint32_t letter = text_next(&cursor);
    if (letter >= 'a' && letter <= 'z')
        letter -= 32;
    if ((letter >= 0xe0 && letter <= 0xf6) || (letter >= 0xf8 && letter <= 0xfe))
        letter -= 32;
    if (letter >= '0' && letter <= '9')
        return '#';
    return letter;
}

void browser_jump_initial(App *app, int direction)
{
    if (!direction || app->scan_directory || app->entry_count < 2)
        return;
    int selected = app->locations[app->category].selected;
    uint32_t initial = entry_initial(app, selected);
    int step = direction < 0 ? -1 : 1;
    for (int count = 1; count < app->entry_count; count++) {
        int target = (selected + step * count + app->entry_count) % app->entry_count;
        if (entry_initial(app, target) == initial)
            continue;
        if (step < 0) {
            uint32_t previous = entry_initial(app, target);
            while (target > 0 && entry_initial(app, target - 1) == previous)
                target--;
        }
        app_move(app, target - selected);
        return;
    }
}
