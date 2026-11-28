#ifndef FRONTEND_LAYOUT_H
#define FRONTEND_LAYOUT_H
#include "font_pack.h"
#include "themes.h"
#include "types.h"

/* Shared by rendering, scrolling and touch input so their row bounds agree. */
#define UI_PANEL_VISIBLE_ROWS 7
#define UI_LIST_BOTTOM 169
#define UI_BROWSE_IDLE_FRAMES 12
#define UI_ARTWORK_IDLE_FRAMES 2
#define UI_ICON_IDLE_FRAMES 1

static inline bool layout_has_path(int category)
{
    return category != CATEGORY_HOME && category != CATEGORY_SYSTEM;
}

static inline int layout_list_top(int category)
{
    return layout_has_path(category) ? 41 : 25;
}

#define UI_SHELF_CARD_HEIGHT 84
static inline int layout_shelf_icon_size(const App *app)
{
    return app->icon_size == ICON_LARGE ? 48 : app->icon_size == ICON_MEDIUM ? 40 : 32;
}

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
           : app->icon_size == ICON_MEDIUM                              ? 3
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
    return app->icon_size == ICON_LARGE ? 32 : app->icon_size == ICON_MEDIUM ? 24 : 16;
}
static inline int layout_text_line_height(void)
{
    unsigned height = font_height(font_current());
    return height < 12 ? 13 : (int)height + 1;
}

static inline int layout_text_height(const App *app)
{
    int height = (int)font_height(font_current());
    if (app->browser_style == BROWSER_EXTENDED) {
        height += layout_text_line_height();
        return height < 26 ? 26 : height;
    }
    return height < 12 ? 12 : height;
}

static inline int layout_row_height(const App *app)
{
    if (app->browser_style == BROWSER_SHELF)
        return UI_LIST_BOTTOM - layout_list_top(app->category);
    unsigned padding = app->themes.active.row_padding;
    if (padding > 12)
        padding = 0;
    if (layout_is_grid(app))
        return (app->icon_size == ICON_LARGE    ? 52
                : app->icon_size == ICON_MEDIUM ? 44
                                                : 20) +
               (int)padding;

    int text = layout_text_height(app);
    int content = text;
    int minimum = text + 2; /* Keep glyphs clear of the row's top/bottom stroke. */
    if (layout_has_icons(app)) {
        int icon = layout_icon_size(app);
        if (content < icon)
            content = icon;
        /* The icon bezel extends two pixels beyond the image on each side. */
        int framed = themes_component(&app->themes.active, THEME_ICON_FRAME) ? 4 : 2;
        if (minimum < icon + framed)
            minimum = icon + framed;
    } else if (minimum < 16)
        minimum = 16;
    if (minimum < content + (int)padding)
        minimum = content + (int)padding;

    /* Home/System have no path line. Fit complete rows to each tab's actual
     * viewport, then share spare pixels between them instead of leaving a
     * large empty strip above the footer. Grid/Shelf retain their tile geometry. */
    int available = UI_LIST_BOTTOM - layout_list_top(app->category);
    int rows = available / minimum;
    return rows > 0 ? available / rows : minimum;
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
