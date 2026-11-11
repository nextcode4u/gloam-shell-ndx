/* User folder tabs. Configuration is bounded and loaded before saved navigation. */
#include "tabs.h"
#include "paths.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TABS_FILE_LIMIT (64 * 1024)

static const char *builtin_ids[CATEGORY_COUNT] = {"home",  "games",  "apps",
                                                  "media", "system", "files"};

static void defaults(App *app)
{
    memset(app->tabs, 0, sizeof app->tabs);
    memset(app->locations, 0, sizeof app->locations);
    app->tab_count = CATEGORY_COUNT;
    app->tab_scroll = 0;
    for (int index = 0; index < CATEGORY_COUNT; index++) {
        Tab *tab = &app->tabs[index];
        path_copy(tab->id, builtin_ids[index], sizeof tab->id);
        path_copy(tab->label, ui_text(&app->language, (UiString)index), sizeof tab->label);
        path_default_category(tab->home, app->root, index);
        path_copy(app->locations[index].path, tab->home, PATH_CAP);
    }
}

int tabs_find(const App *app, const char *id)
{
    for (int index = 0; index < app->tab_count; index++)
        if (!strcasecmp(app->tabs[index].id, id))
            return index;
    return -1;
}

const char *tabs_label(const App *app, int index)
{
    return index >= 0 && index < app->tab_count ? app->tabs[index].label : "";
}

const char *tabs_browse_root(const App *app, int index)
{
    bool library = index == CATEGORY_GAMES || index == CATEGORY_APPS || index == CATEGORY_MEDIA;
    return (library || index >= CATEGORY_COUNT) && index < app->tab_count ? app->tabs[index].home
                                                                          : app->root;
}

void tabs_ensure_visible(App *app)
{
    if (app->category < app->tab_scroll)
        app->tab_scroll = app->category;
    if (app->category >= app->tab_scroll + TAB_VISIBLE_COUNT)
        app->tab_scroll = app->category - TAB_VISIBLE_COUNT + 1;
    int last = app->tab_count > TAB_VISIBLE_COUNT ? app->tab_count - TAB_VISIBLE_COUNT : 0;
    if (app->tab_scroll > last)
        app->tab_scroll = last;
    if (app->tab_scroll < 0)
        app->tab_scroll = 0;
}

static char *trim(char *text)
{
    while (isspace((unsigned char)*text))
        text++;
    size_t length = strlen(text);
    while (length && isspace((unsigned char)text[length - 1]))
        text[--length] = 0;
    return text;
}

static bool valid_id(const char *id)
{
    size_t length = strlen(id);
    if (!length || length >= TAB_ID_CAP)
        return false;
    for (const char *p = id; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
              *p == '_' || *p == '-'))
            return false;
    return true;
}

static void append_tab(App *app, const Tab *tab, bool valid)
{
    if (!valid || !tab->home[0] || tabs_find(app, tab->id) >= 0 || app->tab_count >= TAB_CAP) {
        app->tab_config_skipped++;
        return;
    }
    int index = app->tab_count++;
    app->tabs[index] = *tab;
    if (!tab->label[0])
        path_copy(app->tabs[index].label, tab->id, TAB_LABEL_CAP);
    path_copy(app->locations[index].path, tab->home, PATH_CAP);
}

static void parse_config(App *app, char *data)
{
    Tab pending = {0};
    bool active = false, valid = true, meta = false, roots = false;
    bool version = false, bad_version = false;
    char *context = NULL;
    for (char *line = strtok_r(data, "\n", &context); line; line = strtok_r(NULL, "\n", &context)) {
        if (strlen(line) >= 1024) {
            bad_version = true;
            break;
        }
        char *key = trim(line);
        if (!*key || *key == '#' || *key == ';')
            continue;
        if (*key == '[') {
            if (active)
                append_tab(app, &pending, valid);
            memset(&pending, 0, sizeof pending);
            active = false;
            meta = !strcasecmp(key, "[tabs]");
            roots = !strcasecmp(key, "[roots]");
            size_t length = strlen(key);
            if (!strncasecmp(key, "[tab.", 5) && length > 6 && key[length - 1] == ']') {
                key[length - 1] = 0;
                active = true;
                valid = valid_id(key + 5);
                if (valid) {
                    path_copy(pending.id, key + 5, sizeof pending.id);
                    for (char *p = pending.id; *p; p++)
                        *p = (char)tolower((unsigned char)*p);
                }
            } else if (!meta && !roots)
                app->tab_config_skipped++;
            continue;
        }
        char *value = strchr(key, '=');
        if (!value) {
            if (active)
                valid = false;
            continue;
        }
        *value++ = 0;
        key = trim(key);
        value = trim(value);
        if (meta && !strcasecmp(key, "version")) {
            version = true;
            if (strcmp(value, "1"))
                bad_version = true;
        } else if (roots) {
            int index = tabs_find(app, key);
            char home[PATH_CAP];
            if ((index == CATEGORY_GAMES || index == CATEGORY_APPS || index == CATEGORY_MEDIA) &&
                path_resolve_sd(home, app->root, value)) {
                path_copy(app->tabs[index].home, home, PATH_CAP);
                path_copy(app->locations[index].path, home, PATH_CAP);
            } else
                app->tab_config_skipped++;
        } else if (active && !strcasecmp(key, "label")) {
            if (strlen(value) >= sizeof pending.label)
                valid = false;
            else
                path_copy(pending.label, value, sizeof pending.label);
        } else if (active && (!strcasecmp(key, "path") || !strcasecmp(key, "folder"))) {
            if (!path_resolve_sd(pending.home, app->root, value))
                valid = false;
        }
    }
    if (active)
        append_tab(app, &pending, valid);
    if (!version || bad_version) {
        defaults(app);
        app->tab_config_skipped++;
    }
}

void tabs_load(App *app)
{
    defaults(app);
    app->tab_config_skipped = 0;
    if (!app->storage_ready)
        return;
    char path[PATH_CAP];
    if (!path_preferences_file(path, app->root, "tabs.ini"))
        return;
    FILE *file = fopen(path, "rb");
    if (!file)
        return;
    long size = -1;
    if (fseek(file, 0, SEEK_END) == 0)
        size = ftell(file);
    if (size < 0 || size > TABS_FILE_LIMIT || fseek(file, 0, SEEK_SET) != 0) {
        app->tab_config_skipped++;
        fclose(file);
        return;
    }
    char *data = malloc((size_t)size + 1);
    if (!data) {
        app->tab_config_skipped++;
        fclose(file);
        return;
    }
    bool read = fread(data, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    data[size] = 0;
    if (read && strlen(data) == (size_t)size)
        parse_config(app, data);
    else
        app->tab_config_skipped++;
    free(data);
}
