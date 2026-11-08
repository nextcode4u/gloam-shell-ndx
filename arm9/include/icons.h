#ifndef FRONTEND_ICONS_H
#define FRONTEND_ICONS_H
#include "types.h"

/* Original built-in symbols; installed Nintendo banners are read locally, never bundled. */
bool icon_system_fallback(int target, uint16_t pixels[32 * 32]);
bool icon_is_ds_title(const char *path);
bool icon_load_png(const char *path, uint16_t pixels[32 * 32]);
bool icon_decode_banner(const void *data, size_t bytes, uint16_t pixels[32 * 32]);
bool icon_decode_animation(const void *header, const void *animation, size_t bytes,
                           BrowserIcon *icon);
bool icon_load_banner(const char *path, uint16_t pixels[32 * 32]);
void icons_reset(App *app);
bool icon_load_animation(const char *path, BrowserIcon *icon);
void icon_animate(BrowserIcon *icon, bool reduced_motion);
/* Resolve one uncached visible entry per call; invalid custom PNG may fall back to its banner. */
void icons_tick(App *app);
const BrowserIcon *icons_for_entry(const App *app, int entry_index);
#endif
