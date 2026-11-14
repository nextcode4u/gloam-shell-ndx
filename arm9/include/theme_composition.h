#ifndef FRONTEND_THEME_COMPOSITION_H
#define FRONTEND_THEME_COMPOSITION_H
#include "types.h"

enum {
    TOP_SIDEBAR,
    TOP_SIDEBAR_LEFT,
    TOP_POSTER,
    TOP_SCENE,
    TOP_SPOTLIGHT,
    TOP_GALLERY,
    TOP_JOURNAL,
    TOP_LAYOUT_COUNT
};
typedef struct {
    int x, y, width, height;
} ThemeBox;
typedef struct {
    ThemeBox title, artwork, information;
    int tabs_y, tabs_height;
} ThemeComposition;

/* Named, bounded compositions. Both title metrics and frame outsets fit here;
 * the bottom browser retains its independently shared drawing/touch geometry. */
static inline ThemeComposition theme_composition(const ThemePackage *theme)
{
    bool above =
        theme->tabs_position == 1 || (!theme->tabs_position && theme->top_layout == TOP_GALLERY);
    int tabs_height = theme->tab_style == 1 ? 32 : 20;
    int tabs_y = above ? 26 : 176 - tabs_height;
    int top = above ? tabs_y + tabs_height + 6 : 27;
    int bottom = above ? 173 : tabs_y - 6;
    ThemeComposition result = {
        .title = {12, top, 232, 16}, .tabs_y = tabs_y, .tabs_height = tabs_height};
    int height = bottom - top - 24;
    if (theme->top_layout == TOP_SPOTLIGHT) {
        if (height > 104)
            height = 104;
        result.artwork = (ThemeBox){76, top + 24, 104, height};
    } else if (theme->top_layout == TOP_GALLERY) {
        result.artwork = (ThemeBox){14, top + 24, 92, height};
        result.information = (ThemeBox){120, top + 24, 124, height};
    } else {
        height = bottom - top - 8;
        if (height > 84)
            height = 84;
        result.artwork = (ThemeBox){14, top + 4, 60, height};
        result.title = (ThemeBox){86, top + 2, 158, 16};
        result.information = (ThemeBox){86, top + 26, 158, bottom - top - 26};
    }
    return result;
}
#endif
