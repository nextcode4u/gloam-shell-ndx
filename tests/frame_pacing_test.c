#include "frame_pacing.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void)
{
    /* Each 8 ms decoder slice still services input. Idle paints are separated
     * by two VBlanks; input and completion bypass that delay. */
    for (unsigned elapsed = 0; elapsed < 6; elapsed++) {
        assert(frame_pacing_draw_due(100 + elapsed, 100, true, false) == (elapsed >= 2));
        assert(frame_pacing_draw_due(100 + elapsed, 100, true, true));
        assert(frame_pacing_draw_due(100 + elapsed, 100, false, false));
    }
    assert(!frame_pacing_draw_due(UINT_MAX, UINT_MAX, true, false));
    assert(!frame_pacing_draw_due(0, UINT_MAX, true, false));
    assert(frame_pacing_draw_due(1, UINT_MAX, true, false));
    /* Slow themes start the quiet interval after their transfer completes. */
    assert(!frame_pacing_draw_due(108, 108, true, false));
    assert(frame_pacing_draw_due(110, 108, true, false));
    puts("Frame pacing tests passed");
    return 0;
}
