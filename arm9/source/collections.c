/* Small named shortcut groups. Content is never copied, renamed or deleted. */
#include "collections.h"
#include "browser.h"
#include "paths.h"
#include "system_routes.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static bool identifier(const char *value)
{
    if (!*value || strlen(value) >= 32)
        return false;
    for (; *value; value++)
        if (!isalnum((unsigned char)*value) && *value != '_' && *value != '-')
            return false;
    return true;
}
void collections_load(App *app)
{
    memset(&app->collections, 0, sizeof app->collections);
    app->collection_open = -1;
    if (!app->storage_ready || app->recovery_mode)
        return;
    char path[PATH_CAP];
    if (!path_preferences_file(path, app->root, "collections.ini"))
        return;
    FILE *file = fopen(path, "r");
    if (!file)
        return;
    CollectionState *state = calloc(1, sizeof *state);
    if (!state) {
        fclose(file);
        return;
    }
    char line[1024];
    unsigned bytes = 0, seen[COLLECTION_CAP] = {0};
    int group = -1;
    bool valid = true, version = false;
    while (fgets(line, sizeof line, file)) {
        bytes += strlen(line);
        if (bytes > 65536 || (!strchr(line, '\n') && !feof(file))) {
            valid = false;
            break;
        }
        line[strcspn(line, "\r\n")] = 0;
        if (!*line || *line == ';' || *line == '#')
            continue;
        if (!strcmp(line, "[collections]")) {
            group = -1;
            continue;
        }
        if (!strncmp(line, "[collection.", 12)) {
            size_t n = strlen(line);
            if (n < 14 || line[n - 1] != ']' || state->count == COLLECTION_CAP) {
                valid = false;
                break;
            }
            line[n - 1] = 0;
            const char *id = line + 12;
            if (!identifier(id)) {
                valid = false;
                break;
            }
            for (int i = 0; i < state->count; i++)
                if (!strcmp(state->groups[i].id, id))
                    valid = false;
            if (!valid)
                break;
            group = state->count++;
            path_copy(state->groups[group].id, id, 32);
            path_copy(state->groups[group].label, id, 64);
            continue;
        }
        if (group < 0) {
            if (strcmp(line, "version=1") || version)
                valid = false;
            else
                version = true;
            if (!valid)
                break;
            continue;
        }
        char *value = strchr(line, '=');
        if (!value) {
            valid = false;
            break;
        }
        *value++ = 0;
        NamedCollection *collection = &state->groups[group];
        if (!strcmp(line, "label")) {
            if (!*value || strlen(value) >= 64) {
                valid = false;
                break;
            }
            path_copy(collection->label, value, 64);
            continue;
        }
        if (strncmp(line, "item", 4) || !line[4]) {
            valid = false;
            break;
        }
        char *end;
        unsigned long key = strtoul(line + 4, &end, 10);
        if (strncmp(line, "item", 4) || !line[4] || *end || key >= COLLECTION_ITEM_CAP ||
            (seen[group] & (1u << key))) {
            valid = false;
            break;
        }
        seen[group] |= 1u << key;
        if (system_destination(value) >= 0)
            path_copy(path, value, PATH_CAP);
        else if (!path_resolve_sd(path, app->root, value)) {
            valid = false;
            break;
        }
        int index = 0;
        while (index < state->item_count && strcmp(state->paths[index], path))
            index++;
        if (index == state->item_count) {
            if (index == COLLECTION_ITEM_CAP) {
                valid = false;
                break;
            }
            path_copy(state->paths[state->item_count++], path, PATH_CAP);
        }
        bool duplicate = false;
        for (int i = 0; i < collection->count; i++)
            if (collection->items[i] == index)
                duplicate = true;
        if (!duplicate)
            collection->items[collection->count++] = (unsigned char)index;
    }
    valid &= version && !ferror(file);
    fclose(file);
    if (valid)
        app->collections = *state;
    else
        app->collections.invalid = true;
    free(state);
}
void collections_scan(App *app)
{
    if (!app->storage_ready || app->collection_open < 0 ||
        app->collection_open >= app->collections.count)
        return;
    const NamedCollection *group = &app->collections.groups[app->collection_open];
    for (int i = 0; i < group->count; i++) {
        const char *path = app->collections.paths[group->items[i]];
        int destination = system_destination(path);
        if (destination >= 0) {
            browser_add_entry(app, system_destination_name(destination), path, ITEM_SYSTEM,
                              destination);
            continue;
        }
        struct stat status;
        bool missing = stat(path, &status) != 0;
        const char *name = strrchr(path, '/');
        char label[256];
        snprintf(label, sizeof label, "%s%.*s", missing ? "Missing: " : "", 240,
                 name ? name + 1 : path);
        browser_add_entry(app, label, path,
                          missing                   ? ITEM_MISSING
                          : S_ISDIR(status.st_mode) ? ITEM_DIRECTORY
                                                    : ITEM_FILE,
                          0);
    }
}
