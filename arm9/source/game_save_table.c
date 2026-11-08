/* Private read-only adapter. The database and GPL notices live under third_party. */
#include "game_options.h"
typedef uint32_t u32;
typedef struct ROMListEntry ROMListEntry;
#include "../../third_party/rom-save-table/ROMList.h"

uint32_t game_option_save_bytes(int choice)
{
    static const uint32_t sizes[] = {0,      512,     8192,    65536,    131072,  262144,
                                     524288, 1048576, 8388608, 16777216, 67108864};
    return choice >= 0 && choice < 11 ? sizes[choice] : 0;
}
uint32_t game_save_size(uint32_t code, bool *known)
{
    size_t low = 0, high = sizeof ROMList / sizeof ROMList[0];
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (ROMList[middle].GameCode < code)
            low = middle + 1;
        else
            high = middle;
    }
    *known = low < sizeof ROMList / sizeof ROMList[0] && ROMList[low].GameCode == code &&
             ROMList[low].SaveMemType <= 10;
    return *known ? game_option_save_bytes(ROMList[low].SaveMemType) : 0;
}
