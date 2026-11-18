#ifndef FRONTEND_TOP_CONTENT_H
#define FRONTEND_TOP_CONTENT_H
#include "types.h"
TopContentMode top_content_mode(const App *app);
const char *top_content_name(unsigned mode);
const char *top_content_id(unsigned mode);
void top_content_tick(App *app);
/* Reuse the existing three-image cache; negative indexes represent empty cards. */
void top_content_samples(const App *app, int indexes[3]);
typedef void (*TopContentIcon)(const App *, uint16_t *, int, int, int, int, uint16_t);
void top_content_render(const App *app, uint16_t *pixels, TopContentIcon icon);
#endif
