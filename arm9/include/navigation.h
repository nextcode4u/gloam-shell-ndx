#ifndef FRONTEND_NAVIGATION_H
#define FRONTEND_NAVIGATION_H
#include "types.h"

typedef struct {
    unsigned delay_frames;
    unsigned interval_frames;
} ScrollTiming;

/* Held-button repeat only. Individual presses always move one item immediately. */
static inline ScrollTiming navigation_scroll_timing(ScrollSpeed speed)
{
    static const ScrollTiming timings[SCROLL_SPEED_COUNT] = {{24, 8}, {18, 4}, {6, 1}};
    return timings[(unsigned)speed < SCROLL_SPEED_COUNT ? speed : SCROLL_NORMAL];
}

/* Value editors and tab/letter navigation retain a controllable repeat rate. */
static inline ScrollSpeed navigation_scroll_speed(const App *app, bool fine_control)
{
    return fine_control || app->panel != PANEL_NONE || app->settings.open ||
                   app->game_editor.open || app->dashboard.open || app->association_editor.open ||
                   app->rail_focus || app->search
               ? SCROLL_NORMAL
               : app->scroll_speed;
}
#endif
