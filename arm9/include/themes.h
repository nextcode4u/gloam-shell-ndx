#ifndef FRONTEND_THEMES_H
#define FRONTEND_THEMES_H
#include "file_types.h"
#include "types.h"
#define THEME_LIST_TOP 40
#define THEME_ROW_HEIGHT 17
#define THEME_VISIBLE_ROWS 7
#define THEME_TAB_ICON_FIRST (PLACEHOLDER_COUNT + 7)
#define THEME_ICON_COUNT (THEME_TAB_ICON_FIRST + CATEGORY_COUNT + 1)
void themes_icons_load(const ThemePackage *package, bool preview);
void themes_icons_accept(void);
void themes_icons_cancel(void);
const uint16_t *themes_icon(const ThemePackage *package, int index);
typedef enum {
    THEME_HEADER,
    THEME_FOOTER,
    THEME_PANEL,
    THEME_ROW_SELECTED,
    THEME_TAB_SELECTED,
    THEME_CARD,
    THEME_CARD_SELECTED,
    THEME_ROW,
    THEME_ICON_FRAME,
    THEME_ARTWORK_FRAME,
    THEME_BATTERY_EMPTY,
    THEME_BATTERY_LOW,
    THEME_BATTERY_HALF,
    THEME_BATTERY_HIGH,
    THEME_BATTERY_FULL,
    THEME_BATTERY_CHARGING,
    THEME_BATTERY_UNKNOWN,
    THEME_WIRELESS_IDLE,
    THEME_UPDATE_AVAILABLE,
    THEME_CHECK_OFF,
    THEME_CHECK_ON,
    THEME_TOGGLE_OFF,
    THEME_TOGGLE_ON,
    THEME_ARROW_LEFT,
    THEME_ARROW_RIGHT,
    THEME_ARROW_UP,
    THEME_ARROW_DOWN,
    THEME_SCROLL_TRACK,
    THEME_SCROLL_THUMB,
    THEME_BUSY,
    THEME_COMPONENT_COUNT
} ThemeComponent;
ThemeComponent themes_battery_component(int level, bool charging);
void themes_components_load(const ThemePackage *package, bool preview);
void themes_components_accept(void);
void themes_components_cancel(void);
const uint16_t *themes_component(const ThemePackage *package, ThemeComponent component);
/* Optional packed 2-bit coverage. NULL keeps the binary-alpha fast path. */
const uint8_t *themes_component_alpha(const ThemePackage *package, ThemeComponent component);
/* Optional screen-anchored material and its 16 prepared RGB555 fill colors. */
const uint8_t *themes_component_material(const ThemePackage *package, ThemeComponent component,
                                         const uint16_t **palette);
#define THEME_ACCENT_COUNT 16
const char *themes_accent_name(int index);
void themes_init(App *app);
void themes_open(App *app);
void themes_move(App *app, int direction);
void themes_preview(App *app);
void themes_tick(App *app);
/* Partial preview banks stay hidden until their whole transaction is ready. */
bool themes_preview_visible(void);
void themes_cancel(App *app);
void themes_apply(App *app);
void themes_touch(App *app, int x, int y);
void themes_render(const App *app, uint16_t *top, uint16_t *bottom);
ThemePalette themes_palette(const App *app);
void themes_accent_override(App *app);
void themes_background_load(const ThemePackage *package, bool preview);
void themes_background_accept(void);
void themes_background_cancel(void);
void themes_background_draw(const ThemePackage *package, int screen, uint16_t *pixels);
void themes_background_scene_draw(const ThemePackage *package, uint16_t *pixels);
static inline unsigned themes_content_opacity(const ThemePackage *package, int screen)
{
    return package->screen_opacity[screen] <= 100 ? package->screen_opacity[screen]
                                                  : package->content_opacity;
}
bool themes_background_set(App *app, int screen, const char *path);
/* Zero means inherit, so personal accent changes still update default tab ink. */
static inline uint16_t themes_selection_text(const ThemePackage *package)
{
    return package->selection_text ? package->selection_text : package->palette.text;
}
static inline uint16_t themes_tab_text(const ThemePackage *package)
{
    return package->tab_text ? package->tab_text : package->palette.ink;
}
/* Optional chrome inks preserve each caller's legacy fallback when omitted. */
static inline uint16_t themes_header_color(const ThemePackage *package, uint16_t fallback)
{
    return package->header_text ? package->header_text : fallback;
}
static inline uint16_t themes_footer_color(const ThemePackage *package, uint16_t fallback)
{
    return package->footer_text ? package->footer_text : fallback;
}
static inline uint16_t themes_header_link_color(const ThemePackage *package)
{
    return package->header_link_text ? package->header_link_text : package->palette.accent;
}
#endif
