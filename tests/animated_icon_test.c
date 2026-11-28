#include "icons.h"
#include "layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static BrowserIcon icon;
static bool measure;
static unsigned opens, end_seeks;
static size_t read_bytes;
FILE *__real_fopen(const char *, const char *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (measure)
        opens++;
    return __real_fopen(path, mode);
}
int __real_fseek(FILE *, long, int);
int __wrap_fseek(FILE *file, long offset, int origin)
{
    if (measure && origin == SEEK_END)
        end_seeks++;
    return __real_fseek(file, offset, origin);
}
size_t __real_fread(void *, size_t, size_t, FILE *);
size_t __wrap_fread(void *data, size_t size, size_t count, FILE *file)
{
    if (measure)
        read_bytes += size * count;
    return __real_fread(data, size, count, file);
}
static App browser;
static void check_visible_static_priority(const char *path)
{
    memset(&browser, 0, sizeof browser);
    browser.storage_ready = true;
    browser.browser_style = BROWSER_CLASSIC;
    browser.entry_count = 2;
    browser.entries[0].kind = browser.entries[1].kind = ITEM_INSTALLED_TITLE;
    snprintf(browser.entries[0].path, sizeof browser.entries[0].path, "%s", path);
    const char *name = strrchr(path, '/');
    assert(name);
    snprintf(browser.entries[1].path, sizeof browser.entries[1].path, "%.*s/.%s",
             (int)(name - path), path, name);
    icons_reset(&browser);
    for (int i = 0; i < 2 * UI_ICON_IDLE_FRAMES; i++)
        icons_tick(&browser);
    const BrowserIcon *first = icons_for_entry(&browser, 0);
    const BrowserIcon *second = icons_for_entry(&browser, 1);
    assert(first && second && first != second);
    assert(!first->animation_count && !second->animation_count);
    assert(first->animation_pending && second->animation_pending);
    for (int i = 0; i < UI_ICON_IDLE_FRAMES; i++)
        icons_tick(&browser);
    assert(first->animation_count == 2 && !second->animation_count);
    icons_tick(&browser);
    assert(second->animation_count == 2);
}
static void check_browser_staging(const char *path)
{
    memset(&browser, 0, sizeof browser);
    browser.storage_ready = true;
    browser.browser_style = BROWSER_LIST;
    browser.entry_count = 1;
    browser.entries[0].kind = ITEM_INSTALLED_TITLE;
    snprintf(browser.entries[0].path, sizeof browser.entries[0].path, "%s", path);
    icons_reset(&browser);
    unsigned before_opens = opens;
    size_t before_bytes = read_bytes;
    measure = true;
    for (int i = 0; i < UI_ICON_IDLE_FRAMES; i++)
        icons_tick(&browser);
    const BrowserIcon *shown = icons_for_entry(&browser, 0);
    assert(shown && shown->ready && shown->animation_pending && !shown->animation_count);
    assert(opens == before_opens + 1 && read_bytes - before_bytes < 1024);
    browser.navigation_held = true;
    for (int i = 0; i < 30; i++)
        icons_tick(&browser);
    assert(opens == before_opens + 1);
    browser.navigation_held = false;
    browser.reduced_motion = true;
    for (int i = 0; i < 30; i++)
        icons_tick(&browser);
    assert(opens == before_opens + 1 && shown->animation_pending);
    browser.reduced_motion = false;
    for (int i = 0; i < UI_ICON_IDLE_FRAMES; i++)
        icons_tick(&browser);
    assert(opens == before_opens + 2 && shown->animation_count == 2 && !shown->animation_pending);
    assert(read_bytes - before_bytes < 6 * 1024 && !end_seeks);
    measure = false;

    /* The source changing between static and animation reads retains the icon,
     * drops the stale animation and does not retry it every idle frame. */
    icons_reset(&browser);
    for (int i = 0; i < UI_ICON_IDLE_FRAMES; i++)
        icons_tick(&browser);
    assert(icons_for_entry(&browser, 0)->animation_pending);
    assert(!truncate(path, 64 * 1024 * 1024 - 1));
    unsigned before_reads = browser.icon_reads;
    for (int i = 0; i < 30; i++)
        icons_tick(&browser);
    shown = icons_for_entry(&browser, 0);
    assert(shown && !shown->animation_pending && !shown->animation_count);
    assert(browser.icon_reads == before_reads + 1);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    /* A large sparse title must cost the same bounded banner read as a small
     * one, with one open and no walk to EOF on a FAT-backed device. */
    assert(!truncate(argv[1], 64 * 1024 * 1024));
    measure = true;
    assert(icon_load_title(argv[1], &icon) && icon.animation_count == 2);
    measure = false;
    assert(opens == 1 && !end_seeks && read_bytes < 6 * 1024);
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
    check_visible_static_priority(argv[1]);
    check_browser_staging(argv[1]);
    FILE *f = fopen(argv[1], "r+b");
    assert(f && !fseek(f, 0x200 + 8, SEEK_SET));
    fputc(0, f);
    fputc(0, f);
    assert(!fclose(f));
    assert(!icon_load_animation(argv[1], &icon) && !icon.animation_count);
    assert(icon_load_banner(argv[1],
                            icon.pixels)); /* A bad animation retains static-banner support. */
    assert(icon_load_title(argv[1], &icon) && !icon.animation_count);
    assert(!truncate(argv[1], 0x200 + 0x23bf));
    assert(icon_load_title(argv[1], &icon) && !icon.animation_count);
    assert(!truncate(argv[1], 0x200 + 0x83f));
    assert(!icon_load_title(argv[1], &icon) && !icon.animation_count);
    puts("PASS: native DSi animation CRC/bounds, durations, loop, palette/bitmap selection, flips, "
         "transparency and reduced-motion freeze");
}
