/* Shared directional navigation and touch geometry for the browser's grid. */
#include "grid.h"
#include "app.h"
#include "layout.h"

void browser_direction(App *app, int horizontal, int vertical)
{
    if (!app->entry_count)
        return;
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
