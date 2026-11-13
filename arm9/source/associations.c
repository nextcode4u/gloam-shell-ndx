/* Bounded INI launch profiles shared by icons and homebrew routing. */
#include "associations.h"
#include "arguments.h"
#include "icons.h"
#include "item_preferences.h"
#include "paths.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static char *trim(char *text)
{
    while (isspace((unsigned char)*text))
        text++;
    size_t length = strlen(text);
    while (length && isspace((unsigned char)text[length - 1]))
        text[--length] = 0;
    return text;
}

static bool load_profile(const App *app, const char *path, const char *filename,
                         Association *profile)
{
    profile->use_argv = true;
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    if (fseek(file, 0, SEEK_END) != 0 || ftell(file) > 4096) {
        fclose(file);
        return false;
    }
    rewind(file);
    char line[1024], fallback[PATH_CAP] = "", mode[PATH_CAP] = "";
    bool section = false, valid = true;
    snprintf(profile->extensions, sizeof profile->extensions, "%.*s", (int)strlen(filename) - 4,
             filename);
    while (fgets(line, sizeof line, file)) {
        if (!strchr(line, '\n') && !feof(file)) {
            valid = false;
            break;
        }
        char *key = trim(line);
        if (!*key || *key == ';' || *key == '#')
            continue;
        if (*key == '[') {
            section = !strcasecmp(key, "[plugin]");
            continue;
        }
        if (!section)
            continue;
        char *value = strchr(key, '=');
        if (!value)
            continue;
        *value++ = 0;
        key = trim(key);
        value = trim(value);
        if (!strcasecmp(key, "version") && strcmp(value, "1"))
            valid = false;
        if (!strcasecmp(key, "extensions")) {
            if (strlen(value) >= sizeof profile->extensions)
                valid = false;
            path_copy(profile->extensions, value, sizeof profile->extensions);
        } else if (!strcasecmp(key, "name")) {
            /* Optional display metadata must not disable a working launch profile. */
            if (strlen(value) < sizeof profile->name)
                path_copy(profile->name, value, sizeof profile->name);
            else
                profile->name[0] = 0;
        } else if (!strcasecmp(key, "content_root")) {
            profile->content_root[0] = 0;
            if (*value && !path_resolve_sd(profile->content_root, app->root, value))
                valid = false;
        } else if (!strcasecmp(key, "application") || !strcasecmp(key, "path")) {
            if (strlen(value) >= sizeof fallback)
                valid = false;
            path_copy(fallback, value, sizeof fallback);
        } else if (!strcasecmp(key, "cover")) {
            if (!path_resolve_sd(profile->cover, app->root, value))
                profile->cover[0] = 0;
        } else if (!strcasecmp(key, "icon")) {
            if (!path_resolve_sd(profile->icon, app->root, value) ||
                strcasecmp(path_extension(profile->icon), "png"))
                profile->icon[0] = 0;
        } else if (!strcasecmp(key, "argv") || !strcasecmp(key, "bootstrap") ||
                   !strcasecmp(key, "hbargv") || !strcasecmp(key, "disabled")) {
            if (strcmp(value, "0") && strcmp(value, "1"))
                valid = false;
            bool enabled = !strcmp(value, "1");
            if (!strcasecmp(key, "argv"))
                profile->use_argv = enabled;
            else if (!strcasecmp(key, "bootstrap"))
                profile->bootstrap = enabled;
            else if (!strcasecmp(key, "hbargv"))
                profile->hbargv = enabled;
            else
                profile->disabled = enabled;
        } else if (!strcasecmp(key, app->dsi_mode ? "dsipath" : "ntrpath")) {
            if (strlen(value) >= sizeof mode)
                valid = false;
            path_copy(mode, value, sizeof mode);
        }
    }
    if (ferror(file))
        valid = false;
    fclose(file);
    return valid && (profile->disabled ||
                     (path_resolve_sd(profile->application, app->root, *mode ? mode : fallback) &&
                      icon_is_ds_title(profile->application)));
}

static int compare_names(const void *left, const void *right)
{
    return strcmp(left, right);
}

static void load_user(App *app);

void associations_load(App *app)
{
    app->association_count = 0;
    load_user(app);
    char directory[PATH_CAP];
    if (!path_join(directory, app->root, "_nds/dsi-frontend/plugins"))
        return;
    DIR *folder = opendir(directory);
    if (!folder)
        return;
    char names[ASSOCIATION_CAP][128];
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(folder))) {
        if (entry->d_name[0] == '.' || strcasecmp(path_extension(entry->d_name), "ini") ||
            strlen(entry->d_name) >= sizeof names[0])
            continue;
        /* Keep the alphabetically first bounded set, independent of FAT order. */
        if (count < ASSOCIATION_CAP) {
            path_copy(names[count++], entry->d_name, sizeof names[0]);
        } else if (strcmp(entry->d_name, names[count - 1]) < 0) {
            path_copy(names[count - 1], entry->d_name, sizeof names[0]);
        }
        qsort(names, count, sizeof names[0], compare_names);
    }
    closedir(folder);
    for (int i = 0; i < count; i++) {
        char path[PATH_CAP];
        Association profile = {0};
        if (path_join(path, directory, names[i]) && load_profile(app, path, names[i], &profile))
            app->associations[app->association_count++] = profile;
    }
}

static size_t extension_match(const char *list, const char *path)
{
    size_t path_length = strlen(path), best = 0;
    while (*list) {
        while (*list == ',' || isspace((unsigned char)*list))
            list++;
        if (!*list)
            break;
        if (*list == '.')
            list++;
        const char *start = list;
        while (*list && *list != ',' && !isspace((unsigned char)*list))
            list++;
        size_t length = (size_t)(list - start);
        if (length > best && path_length > length && path[path_length - length - 1] == '.' &&
            !strncasecmp(start, path + path_length - length, length))
            best = length;
    }
    return best;
}

static size_t folder_match(const App *app, const Association *profile, const char *path)
{
    if (!*profile->content_root || !path_is_within_root(path, app->root))
        return 0;
    size_t length = strlen(profile->content_root);
    /* FAT paths are case-insensitive, but /Books must not match /Bookshelf. */
    if (strncasecmp(path, profile->content_root, length) || strlen(path) <= length ||
        (profile->content_root[length - 1] != '/' && path[length] != '/'))
        return 0;
    return length;
}

static bool same_handler(const Association *left, const Association *right)
{
    return !strcasecmp(left->application, right->application) &&
           left->use_argv == right->use_argv && left->bootstrap == right->bootstrap &&
           left->hbargv == right->hbargv;
}

bool association_registered(const App *app, const char *path)
{
    if (association_user(app, path_extension(path)))
        return true;
    for (int i = 0; i < app->association_count; i++)
        if (extension_match(app->associations[i].extensions, path))
            return true;
    return false;
}

static int profile_choices(const App *app, const char *path,
                           const Association *choices[ASSOCIATION_CAP])
{
    size_t suffix = 0, folder = 0;
    for (int i = 0; i < app->association_count; i++) {
        const Association *profile = &app->associations[i];
        size_t length = extension_match(profile->extensions, path);
        if (!length)
            continue;
        size_t scope = folder_match(app, profile, path);
        if (profile->disabled && *profile->content_root && !scope)
            continue;
        if (length > suffix) {
            suffix = length;
            folder = 0;
        }
        if (length == suffix && scope > folder)
            folder = scope;
    }
    int count = 0;
    for (int i = 0; suffix && i < app->association_count; i++) {
        const Association *profile = &app->associations[i];
        if (extension_match(profile->extensions, path) != suffix)
            continue;
        size_t scope = folder_match(app, profile, path);
        if (folder && scope != folder)
            continue;
        if (profile->disabled) {
            if (!*profile->content_root || scope)
                return 0;
            continue;
        }
        bool duplicate = false;
        for (int j = 0; j < count; j++)
            duplicate |= same_handler(profile, choices[j]);
        if (!duplicate)
            choices[count++] = profile;
    }
    return count;
}

int association_choices(const App *app, const char *path,
                        const Association *choices[ASSOCIATION_CAP])
{
    const Association *custom = association_user(app, path_extension(path));
    if (custom) {
        if (custom->disabled)
            return 0;
        choices[0] = custom;
        return 1;
    }
    return profile_choices(app, path, choices);
}

const Association *association_find(const App *app, const char *path)
{
    const Association *choices[ASSOCIATION_CAP];
    return association_choices(app, path, choices) == 1 ? choices[0] : NULL;
}

const char *association_cover(const App *app, const char *path)
{
    const Association *effective = association_find(app, path);
    if (!effective)
        return NULL;
    if (*effective->cover)
        return effective->cover;
    /* Personal application choices retain the folder-resolved artwork default. */
    const Association *defaults[ASSOCIATION_CAP];
    if (profile_choices(app, path, defaults) == 1 && *defaults[0]->cover)
        return defaults[0]->cover;
    return NULL;
}

static bool icon_source(const App *app, const char *path, char output[PATH_CAP], bool custom,
                        bool file)
{
    ItemPreferences item;
    bool assigned = item_preferences_load(app->root, path, &item);
    if (custom && assigned && *item.icon) {
        path_copy(output, item.icon, PATH_CAP);
        return true;
    }
    if (!file)
        return false;
    if (assigned && *item.application) {
        path_copy(output, item.application, PATH_CAP);
        return true;
    }
    char target[PATH_CAP];
    if (!strcasecmp(path_extension(path), "argv")) {
        ArgumentList args;
        char error[160];
        if (!arguments_read(path, &args, error) ||
            !path_resolve_sd(target, app->root, arguments_get(&args, 0)))
            return false;
        path = target;
    }
    if (icon_is_ds_title(path)) {
        path_copy(output, path, PATH_CAP);
        return true;
    }
    const Association *profile = association_find(app, path);
    if (!profile)
        return false;
    path_copy(output, custom && *profile->icon ? profile->icon : profile->application, PATH_CAP);
    return true;
}

bool association_icon_source(const App *app, const Entry *entry, char output[PATH_CAP])
{
    return icon_source(app, entry->path, output, true, entry->kind == ITEM_FILE);
}
bool association_icon_application(const App *app, const char *path, char output[PATH_CAP])
{
    return icon_source(app, path, output, false, true);
}

bool association_extension_valid(const char *extension)
{
    if (!*extension || strlen(extension) >= EXTENSION_CAP || !strcasecmp(extension, "argv") ||
        icon_is_ds_title(extension) || !strcasecmp(extension, "nds") ||
        !strcasecmp(extension, "dsi") || !strcasecmp(extension, "srl") ||
        !strcasecmp(extension, "app") || !strcasecmp(extension, "dsi_mode"))
        return false;
    for (; *extension; extension++)
        if (!isalnum((unsigned char)*extension) && *extension != '_')
            return false;
    return true;
}
static bool user_path(char path[PATH_CAP], const App *app, const char *extension,
                      const char *suffix)
{
    if (!association_extension_valid(extension))
        return false;
    char relative[96], lower[EXTENSION_CAP];
    for (size_t i = 0; i <= strlen(extension); i++)
        lower[i] = (char)tolower((unsigned char)extension[i]);
    snprintf(relative, sizeof relative, "plugins/user/%s.%s", lower, suffix);
    return path_preferences_file(path, app->root, relative);
}
const Association *association_user(const App *app, const char *extension)
{
    for (int i = 0; i < app->user_association_count; i++)
        if (!strcasecmp(app->user_associations[i].extensions, extension))
            return &app->user_associations[i];
    return NULL;
}
static void load_user(App *app)
{
    app->user_association_count = 0;
    char directory[PATH_CAP];
    if (!path_preferences_file(directory, app->root, "plugins/user"))
        return;
    DIR *folder = opendir(directory);
    if (!folder)
        return;
    char names[USER_ASSOCIATION_CAP][EXTENSION_CAP + 5];
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(folder))) {
        char extension[EXTENSION_CAP];
        size_t length = strlen(entry->d_name);
        if (length < 5 || length >= sizeof names[0] ||
            strcasecmp(entry->d_name + length - 4, ".ini"))
            continue;
        snprintf(extension, sizeof extension, "%.*s", (int)length - 4, entry->d_name);
        if (!association_extension_valid(extension))
            continue;
        if (count < USER_ASSOCIATION_CAP)
            path_copy(names[count++], entry->d_name, sizeof names[0]);
        else if (strcmp(entry->d_name, names[count - 1]) < 0)
            path_copy(names[count - 1], entry->d_name, sizeof names[0]);
        qsort(names, count, sizeof names[0], compare_names);
    }
    closedir(folder);
    for (int i = 0; i < count; i++) {
        char path[PATH_CAP], extension[EXTENSION_CAP];
        snprintf(extension, sizeof extension, "%.*s", (int)strlen(names[i]) - 4, names[i]);
        Association profile = {0};
        if (path_join(path, directory, names[i]) && load_profile(app, path, names[i], &profile) &&
            !strcasecmp(profile.extensions, extension))
            app->user_associations[app->user_association_count++] = profile;
    }
}
static bool prepare_user_directory(const App *app)
{
    if (!app->storage_ready || !path_prepare_preferences_directory(app->root))
        return false;
    const char *directories[] = {"plugins", "plugins/user"};
    for (unsigned i = 0; i < 2; i++) {
        char path[PATH_CAP];
        struct stat status;
        if (!path_preferences_file(path, app->root, directories[i]) ||
            (mkdir(path, 0777) &&
             (errno != EEXIST || stat(path, &status) || !S_ISDIR(status.st_mode))))
            return false;
    }
    return true;
}
static bool write_user(App *app, const char *extension, const char *application, bool disabled)
{
    char current[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP], reference[PATH_CAP] = "";
    if (!user_path(current, app, extension, "ini") ||
        !user_path(temporary, app, extension, "tmp") || !user_path(backup, app, extension, "bak") ||
        (!association_user(app, extension) &&
         app->user_association_count >= USER_ASSOCIATION_CAP) ||
        !prepare_user_directory(app))
        return false;
    if (!disabled) {
        if (!path_is_within_root(application, app->root) || !icon_is_ds_title(application))
            return false;
        const char *relative = application + strlen(app->root);
        while (*relative == '/')
            relative++;
        if (!path_resolve_sd(reference, "sd:/", relative))
            return false;
    }
    FILE *file = fopen(temporary, "w");
    if (!file)
        return false;
    fprintf(file,
            "[plugin]\nversion=1\nextensions=%s\napplication=%s\nargv=1\nbootstrap=0\nhbargv="
            "0\ndisabled=%d\n",
            extension, reference, disabled);
    bool written = !ferror(file);
    if (fclose(file))
        written = false;
    if (!written) {
        remove(temporary);
        return false;
    }
    if (path_commit_file(temporary, current, backup) != FILE_COMMIT_OK)
        return false;
    associations_load(app);
    return true;
}
bool association_set_default(App *app, const char *extension, const char *application)
{
    return write_user(app, extension, application, false);
}
bool association_disable(App *app, const char *extension)
{
    return write_user(app, extension, "", true);
}
bool association_clear_default(App *app, const char *extension)
{
    char current[PATH_CAP], backup[PATH_CAP];
    if (!app->storage_ready || !user_path(current, app, extension, "ini") ||
        !user_path(backup, app, extension, "bak"))
        return false;
    if (remove(backup) && errno != ENOENT)
        return false;
    if (remove(current) && errno != ENOENT)
        return false;
    associations_load(app);
    return true;
}
