#ifndef FRONTEND_PICKER_H
#define FRONTEND_PICKER_H
#include "types.h"
#define PICKER_TOP 41
#define PICKER_ROW_HEIGHT 18
#define PICKER_VISIBLE_ROWS 7
bool picker_open(App *app, PickerKind kind);
bool picker_open_audio(App *app);
bool picker_open_configuration(App *app);
bool picker_open_background(App *app, int screen);
bool picker_open_extension(App *app, const char *extension);
void picker_close(App *app);
void picker_move(App *app, int direction);
void picker_scan_begin(App *app);
void picker_scan_step(App *app);
void picker_batch(App *app, int direction);
void picker_back(App *app);
void picker_activate(App *app);
void picker_choose_folder(App *app);
void picker_touch(App *app, int x, int y);
void picker_tick(App *app);
void picker_render(const App *app, uint16_t *top, uint16_t *bottom);
bool picker_clear_assignment(App *app, bool application);
bool picker_clear_icon(App *app);
#endif
