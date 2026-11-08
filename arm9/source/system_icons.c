/* Original 16-pixel symbols, doubled for the 32-pixel banner cache. These keep
 * system actions recognizable when a console's native banner is unavailable. */
#include "icons.h"
#include <string.h>

bool icon_system_fallback(int target, uint16_t pixels[32 * 32])
{
    static const uint16_t masks[7][16] = {
        /* Settings: a gear with a clear center. */
        {0, 0x0180, 0x03c0, 0x1bd8, 0x1ff8, 0x0e70, 0x3c3c, 0x3c3c, 0x3c3c, 0x3c3c, 0x0e70, 0x1ff8,
         0x1bd8, 0x03c0, 0x0180, 0},
        /* Camera: body, shutter and lens. */
        {0, 0, 0x0f00, 0x1f80, 0x7ffe, 0x6006, 0x6336, 0x6786, 0x6cc6, 0x6cc6, 0x6786, 0x6336,
         0x6006, 0x7ffe, 0, 0},
        /* Sound: a pair of joined musical notes. */
        {0, 0, 0x01f8, 0x07f8, 0x0618, 0x0618, 0x0618, 0x0618, 0x0618, 0x0678, 0x1e78, 0x3e70,
         0x3c00, 0x1800, 0, 0},
        /* PictoChat: overlapping speech bubbles. */
        {0, 0, 0x7fc0, 0x4040, 0x5540, 0x4040, 0x7fc0, 0x103e, 0x1422, 0x182a, 0x0022, 0x003e,
         0x0008, 0x0018, 0, 0},
        /* Download Play: incoming arrow over a handheld screen. */
        {0, 0x0180, 0x0180, 0x0180, 0x07e0, 0x03c0, 0x0180, 0, 0x3ffc, 0x2004, 0x27e4, 0x2424,
         0x27e4, 0x2004, 0x3ffc, 0},
        /* System menu: a home outline. */
        {0, 0, 0x0180, 0x03c0, 0x0660, 0x0c30, 0x1818, 0x300c, 0x6006, 0x1008, 0x1188, 0x1188,
         0x1188, 0x1ff8, 0, 0},
        /* Slot-1: cartridge, label and contacts. */
        {0, 0, 0x1ff8, 0x3008, 0x27e8, 0x2428, 0x2428, 0x27e8, 0x2008, 0x2008, 0x2008, 0x2aa8,
         0x2aa8, 0x3ff8, 0, 0},
    };
    static const uint16_t colors[7] = {
        0x8000 | 22 | (24 << 5) | (27 << 10), 0x8000 | 30 | (14 << 5) | (12 << 10),
        0x8000 | 12 | (26 << 5) | (27 << 10), 0x8000 | 29 | (23 << 5) | (9 << 10),
        0x8000 | 12 | (21 << 5) | (31 << 10), 0x8000 | 16 | (27 << 5) | (16 << 10),
        0x8000 | 23 | (20 << 5) | (29 << 10),
    };
    memset(pixels, 0, 32 * 32 * sizeof *pixels);
    if (target < 0 || target >= 7)
        return false;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            if (masks[target][y / 2] & (0x8000u >> (x / 2)))
                pixels[y * 32 + x] = colors[target];
    return true;
}
