#include "icons.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static BrowserIcon icon;
int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(icon_load_banner(argv[1], icon.pixels));
    assert(icon_load_animation(argv[1], &icon) && icon.animation_count == 2);
    assert(icon.pixels[0] == 0x801F && icon.pixels[1023] == 0x801F);
    icon_animate(&icon, false);
    assert(icon.animation_index == 0);
    icon_animate(&icon, false);
    assert(icon.animation_index == 1);
    assert(icon.pixels[0] == 0 && icon.pixels[1023] == 0xFC00);
    icon_animate(&icon, false);
    assert(icon.animation_index == 0 && icon.pixels[0] == 0x801F);
    icon_animate(&icon, false);
    icon_animate(&icon, false);
    assert(icon.animation_index == 1);
    icon_animate(&icon, true);
    assert(icon.animation_index == 0 && icon.pixels[0] == 0x801F);
    for (int i = 0; i < 200; i++)
        icon_animate(&icon, true);
    assert(icon.animation_index == 0);
    FILE *f = fopen(argv[1], "r+b");
    assert(f && !fseek(f, 0x200 + 8, SEEK_SET));
    fputc(0, f);
    fputc(0, f);
    assert(!fclose(f));
    assert(!icon_load_animation(argv[1], &icon) && !icon.animation_count);
    assert(icon_load_banner(argv[1],
                            icon.pixels)); /* A bad animation retains static-banner support. */
    puts("PASS: native DSi animation CRC/bounds, durations, loop, palette/bitmap selection, flips, "
         "transparency and reduced-motion freeze");
}
