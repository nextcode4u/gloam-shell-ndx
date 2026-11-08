#ifndef FRONTEND_SETTINGS_H
#define FRONTEND_SETTINGS_H
#include "types.h"

typedef struct {
    const char *label;
    const char *help;
    bool adjustable;
    int id;
} SettingsOption;

#define SETTINGS_SECTION_COUNT 7
#define SETTINGS_VISIBLE_ROWS 7
#define SETTINGS_LIST_TOP 41
#define SETTINGS_ROW_HEIGHT 18

void settings_open(App *app);
void settings_close(App *app);
void settings_back(App *app);
void settings_move(App *app, int direction);
void settings_section(App *app, int direction);
void settings_adjust(App *app, int direction);
void settings_activate(App *app);
void settings_touch(App *app, int x, int y);
const char *settings_section_name(int section);
int settings_option_count(int section);
const SettingsOption *settings_option(int section, int index);
void settings_value(const App *app, const SettingsOption *option, char *out, size_t capacity);
void settings_format_clock(char out[12], int hour, int minute, bool twelve_hour);
void settings_render(const App *app, uint16_t *top, uint16_t *bottom);
#endif
