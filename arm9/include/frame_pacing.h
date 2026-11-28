#ifndef FRONTEND_FRAME_PACING_H
#define FRONTEND_FRAME_PACING_H
#include <stdbool.h>

/* Idle cover decoding can advance without repainting an unchanged screen after
 * every slice. Measure from the completed display transfer, not render start:
 * an expensive theme must not consume the whole quiet interval itself. */
static inline bool frame_pacing_draw_due(unsigned vblank, unsigned last_presented,
                                         bool decoder_progress, bool interaction)
{
    return !decoder_progress || interaction || vblank - last_presented >= 2;
}
#endif
