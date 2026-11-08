#ifndef FRONTEND_GAME_OPTIONS_H
#define FRONTEND_GAME_OPTIONS_H
#include "types.h"
#define GAME_INHERIT (-2)
void game_options_defaults(GameOptions *options, bool inherit);
bool game_options_read(const char *root, const char *path, GameOptions *options);
bool game_options_write(const char *root, const char *path, const GameOptions *options);
void game_options_effective(const char *root, const char *path, GameOptions *options);
uint32_t game_save_size(uint32_t code, bool *known);
void game_editor_open(App *app, bool defaults);
void game_editor_move(App *app, int direction);
void game_editor_adjust(App *app, int direction);
bool game_editor_close(App *app);
void game_editor_touch(App *app, int x, int y);
void game_editor_render(const App *app, uint16_t *top, uint16_t *bottom);
const char *game_option_label(int option);
void game_option_value(const GameOptions *options, int option, char output[48]);
uint32_t game_option_save_bytes(int choice);
unsigned game_option_hotkey(int choice);
#endif
