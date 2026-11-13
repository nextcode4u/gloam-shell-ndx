#ifndef FRONTEND_THEMES_H
#define FRONTEND_THEMES_H
#include "file_types.h"
#include "types.h"
#define THEME_ICON_COUNT (PLACEHOLDER_COUNT + 7)
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
    THEME_COMPONENT_COUNT
} ThemeComponent;
ThemeComponent themes_battery_component(int level, bool charging);
void themes_components_load(const ThemePackage *package, bool preview);
void themes_components_accept(void);
void themes_components_cancel(void);
const uint16_t *themes_component(const ThemePackage *package, ThemeComponent component);
#define THEME_ACCENT_COUNT 16
const char *themes_accent_name(int index);
void themes_init(App *app);
void themes_open(App *app);
void themes_move(App *app, int direction);
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
#endif
