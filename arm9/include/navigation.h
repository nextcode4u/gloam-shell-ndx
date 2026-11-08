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
    static const ScrollTiming timings[SCROLL_SPEED_COUNT] = {{24, 8}, {18, 4}, {12, 2}};
    return timings[(unsigned)speed < SCROLL_SPEED_COUNT ? speed : SCROLL_NORMAL];
}
#endif
