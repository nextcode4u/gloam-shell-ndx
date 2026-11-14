#ifndef GLOAM_THEME_MOTION_H
#define GLOAM_THEME_MOTION_H
#include "types.h"
void themes_motion_reset(void);
void themes_motion_tick(const App *app);
const uint16_t *themes_motion_frame(const App *app, const uint8_t **coverage);
void draw_theme_ambient(const App *app, uint16_t *pixels);
#endif
