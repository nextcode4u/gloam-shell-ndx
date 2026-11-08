#include "app.h"
#include "associations.h"
#include "browser.h"
#include "icons.h"
#include "item_preferences.h"
#include "paths.h"
#include "picker.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app;
static int find_entry(const char *name)
{
    for (int i = 0; i < app.entry_count; i++)
        if (!strcmp(app.entries[i].name, name))
            return i;
    assert(!"missing browser entry");
    return -1;
}
static void choose(const char *name)
{
    for (int i = 0; i < app.picker.count; i++)
        if (!strcmp(app.picker.entries[i].name, name)) {
            picker_move(&app, i - app.picker.selected);
            picker_activate(&app);
            return;
        }
    assert(!"missing picker entry");
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    char folder[PATH_CAP], target[PATH_CAP], image[PATH_CAP], broken[PATH_CAP];
    assert(path_join(folder, root, "roms") && !mkdir(folder, 0777));
    assert(path_join(target, folder, "Native.nds"));
    unsigned char header[0x200] = {0}, banner[0x840] = {0};
    header[0x69] = 2;
    banner[0] = 1;
    memset(banner + 0x20, 0x11, 512);
    banner[0x223] = 0x7C; /* Blue RGB555. */
    FILE *file = fopen(target, "wb");
    assert(file && fwrite(header, 1, sizeof header, file) == sizeof header &&
           fwrite(banner, 1, sizeof banner, file) == sizeof banner && !fclose(file));
    assert(path_join(folder, root, "roms/Folder") && !mkdir(folder, 0777));
    assert(path_join(image, root, "Icon.png"));
    assert(path_join(broken, root, "Broken.png"));
    file = fopen(broken, "w");
    assert(file && fputs("Not a PNG", file) >= 0 && !fclose(file));
    app_init(&app, root, true, true);
    app_category(&app, CATEGORY_GAMES);
    app.locations[CATEGORY_GAMES].selected = find_entry("Native.nds");
    assert(picker_open(&app, PICKER_ICON));
    picker_back(&app); /* roms -> SD root */
    choose("Broken.png");
    assert(app.picker.open && *app.picker.status);
    choose("Icon.png");
    assert(!app.picker.open);
    ItemPreferences item;
    assert(item_preferences_load(root, target, &item) && !strcmp(item.icon, image));
    for (int i = 0; i < 4; i++)
        icons_tick(&app);
    const BrowserIcon *icon = icons_for_entry(&app, app.locations[CATEGORY_GAMES].selected);
    assert(icon && !icon->animation_count && icon->pixels[0] == 0 && icon->pixels[1] == 0x801F);
    path_copy(item.cover, image, sizeof item.cover);
    assert(item_preferences_save(root, target, &item));
    assert(picker_clear_icon(&app));
    assert(item_preferences_load(root, target, &item) && !*item.icon && !strcmp(item.cover, image));
    /* A malformed optional icon returns to the native banner. */
    path_copy(item.icon, broken, sizeof item.icon);
    assert(item_preferences_save(root, target, &item));
    icons_reset(&app);
    for (int i = 0; i < 4; i++)
        icons_tick(&app);
    icon = icons_for_entry(&app, app.locations[CATEGORY_GAMES].selected);
    assert(icon && icon->pixels[1] == 0xFC00);
    memset(&item, 0, sizeof item);
    path_copy(item.icon, image, sizeof item.icon);
    assert(item_preferences_save(root, folder, &item));
    app.locations[CATEGORY_GAMES].selected = find_entry("Folder");
    icons_reset(&app);
    for (int i = 0; i < 4; i++)
        icons_tick(&app);
    icon = icons_for_entry(&app, app.locations[CATEGORY_GAMES].selected);
    assert(icon && icon->pixels[0] == 0 && icon->pixels[1] == 0x801F);
    /* Profile artwork never replaces a title banner or the launch target. */
    char profile[PATH_CAP], content[PATH_CAP], plugins[PATH_CAP];
    assert(path_join(plugins, root, "_nds/dsi-frontend/plugins") && !mkdir(plugins, 0777));
    assert(path_join(profile, plugins, "demo.ini"));
    file = fopen(profile, "w");
    assert(file &&
           fputs("[plugin]\nversion=1\nextensions=demo,nds\n"
                 "application=/roms/Native.nds\nicon=/Icon.png\n",
                 file) >= 0 &&
           !fclose(file));
    assert(path_join(content, root, "roms/Paper.demo"));
    file = fopen(content, "w");
    assert(file && !fclose(file));
    associations_load(&app);
    browser_refresh(&app);
    app.locations[CATEGORY_GAMES].selected = find_entry("Paper.demo");
    for (int i = 0; i < 6; i++)
        icons_tick(&app);
    icon = icons_for_entry(&app, app.locations[CATEGORY_GAMES].selected);
    assert(icon && icon->pixels[1] == 0x801F);
    char resolved[PATH_CAP];
    assert(association_icon_application(&app, content, resolved) && !strcmp(resolved, target));
    assert(association_find(&app, content) &&
           !strcmp(association_find(&app, content)->application, target));
    file = fopen(profile, "w");
    assert(file &&
           fputs("[plugin]\nversion=1\nextensions=demo,nds\n"
                 "application=/roms/Native.nds\nicon=/Broken.png\n",
                 file) >= 0 &&
           !fclose(file));
    associations_load(&app);
    icons_reset(&app);
    for (int i = 0; i < 6; i++)
        icons_tick(&app);
    icon = icons_for_entry(&app, app.locations[CATEGORY_GAMES].selected);
    assert(icon && icon->pixels[1] == 0xFC00); /* Invalid profile icon -> app banner. */
    app.locations[CATEGORY_GAMES].selected = find_entry("Native.nds");
    memset(&item, 0, sizeof item);
    assert(item_preferences_save(root, target, &item));
    assert(association_icon_source(&app, app.entries + find_entry("Native.nds"), resolved) &&
           !strcmp(resolved, target));
    puts("PASS: independent custom 32x32 PNG icons, transparency, file/folder assignment, "
         "cover preservation, profile icons, native priority and invalid-image banner fallback");
}
