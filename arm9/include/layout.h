#ifndef FRONTEND_LAYOUT_H
#define FRONTEND_LAYOUT_H
#include "types.h"

/* Shared by rendering, scrolling and touch input so their row bounds agree. */
#define UI_PANEL_VISIBLE_ROWS 7
#define UI_LIST_BOTTOM 169

static inline bool layout_has_path(int category)
{
    return category != CATEGORY_HOME && category != CATEGORY_SYSTEM;
}

static inline int layout_list_top(int category)
{
    return layout_has_path(category) ? 41 : 25;
}

#define UI_SHELF_CARD_HEIGHT 84
#define UI_SHELF_ICON_SIZE 48

static inline int layout_shelf_top(const App *app)
{
    (void)app;
    return 56; /* Reserve the same heading/path space on every tab. */
}

static inline bool layout_is_grid(const App *app)
{
    return app->browser_style == BROWSER_GRID || app->browser_style == BROWSER_SHELF;
}

/* Small icons sit beside longer names; large icons retain square tiles. */
static inline int layout_grid_columns(const App *app)
{
    if (app->browser_style == BROWSER_SHELF)
        return 3;
    unsigned columns = app->themes.active.large_grid_columns;
    return app->icon_size == ICON_LARGE && columns >= 2 && columns <= 4 ? (int)columns
           : app->icon_size == ICON_LARGE                               ? 4
                                                                        : 2;
}
static inline int layout_grid_cell_width(const App *app)
{
    return 244 / layout_grid_columns(app);
}

static inline bool layout_has_icons(const App *app)
{
    return app->browser_style != BROWSER_LIST;
}
static inline int layout_icon_size(const App *app)
{
    return app->browser_style == BROWSER_INDEX ? 16 : app->icon_size == ICON_LARGE ? 32 : 16;
}
static inline int layout_row_height(const App *app)
{
    if (app->browser_style == BROWSER_SHELF)
        return UI_LIST_BOTTOM - layout_list_top(app->category);
    if (app->browser_style == BROWSER_INDEX)
        return 18;
    int base;
    if (app->browser_style == BROWSER_LIST)
        base = 16;
    else if (layout_is_grid(app))
        base = app->icon_size == ICON_LARGE ? 52 : 20;
    else if (app->browser_style == BROWSER_EXTENDED)
        base = app->icon_size == ICON_LARGE ? 40 : 28;
    else
        base = app->icon_size == ICON_LARGE ? 36 : 18;
    unsigned padding = app->themes.active.row_padding;
    return base + (padding <= 12 ? (int)padding : 0);
}
static inline int layout_visible_rows(const App *app)
{
    int rows = (UI_LIST_BOTTOM - layout_list_top(app->category)) / layout_row_height(app);
    return layout_is_grid(app) ? rows * layout_grid_columns(app) : rows;
}
static inline int layout_list_bottom(const App *app)
{
    int rows = layout_visible_rows(app) / (layout_is_grid(app) ? layout_grid_columns(app) : 1);
    return layout_list_top(app->category) + rows * layout_row_height(app);
}
static inline int layout_panel_top(const App *app)
{
    int base = app->panel == PANEL_QUICK || app->panel == PANEL_ACTIONS ? 30 : 45;
    int offset = app->reduced_motion ? 0 : (app->panel_age < 5 ? (5 - app->panel_age) * 4 : 0);
    return base + offset;
}
#endif
