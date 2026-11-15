#ifndef GLOAM_THEME_ASSETS_H
#define GLOAM_THEME_ASSETS_H
#include "types.h"
/* Temporary decode cache, scoped to one controller-owned theme load. */
void theme_assets_begin(const ThemePackage *package);
void theme_assets_end(void);
bool theme_assets_copy(const ThemePackage *package, bool component, unsigned index,
                       uint16_t pixels[1024], uint8_t coverage[256]);
bool theme_assets_background(const ThemePackage *package, unsigned screen,
                             uint16_t pixels[256 * 192], uint16_t **scene, bool *wallpaper_ready);
bool theme_assets_material(const ThemePackage *package, unsigned index,
                           uint8_t shades[256 * 192 / 2]);
#endif
