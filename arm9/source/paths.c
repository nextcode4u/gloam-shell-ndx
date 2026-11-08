/* Shared path policy. Never writes to another frontend's private directory. */
#include "paths.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

void path_copy(char *destination, const char *source, size_t capacity)
{
    if (capacity == 0)
        return;
    size_t length = strlen(source);
    if (length >= capacity)
        length = capacity - 1;
    memmove(destination, source, length);
    destination[length] = '\0';
}

bool path_join(char output[PATH_CAP], const char *directory, const char *filename)
{
    size_t length = strlen(directory);
    const char *separator = length > 0 && directory[length - 1] == '/' ? "" : "/";
    int written = snprintf(output, PATH_CAP, "%s%s%s", directory, separator, filename);
    return written >= 0 && written < PATH_CAP;
}

const char *path_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot ? dot + 1 : "";
}

bool path_is_within_root(const char *path, const char *root)
{
    size_t root_length = strlen(root);
    size_t path_length = strlen(path);
    if (root_length == 0 || path_length < root_length || strncmp(path, root, root_length) != 0)
        return false;
    if (root[root_length - 1] != '/' && path[root_length] != '\0' && path[root_length] != '/')
        return false;

    /* Saved paths use SD-style forward slashes. Reject parent traversal. */
    return strchr(path, '\\') == NULL && strstr(path, "/../") == NULL &&
           !(path_length >= 3 && strcmp(path + path_length - 3, "/..") == 0);
}

void path_default_category(char output[PATH_CAP], const char *root, int category)
{
    static const char *directories[CATEGORY_COUNT] = {"", "roms", "apps", "media", "", ""};
    const char *relative = category >= 0 && category < CATEGORY_COUNT ? directories[category] : "";
    path_join(output, root, relative);
}

/* SD configuration paths share one root policy across tabs and associations. */
bool path_resolve_sd(char output[PATH_CAP], const char *root, const char *value)
{
    if (!*value || strlen(value) >= PATH_CAP || strchr(value, '\\'))
        return false;
    if (!strncasecmp(value, "sd:/", 4))
        value += 4;
    else if (strchr(value, ':'))
        return false;
    char relative[PATH_CAP];
    size_t used = 0;
    while (*value) {
        while (*value == '/')
            value++;
        const char *start = value;
        while (*value && *value != '/') {
            if ((unsigned char)*value < 32 || *value == ':')
                return false;
            value++;
        }
        size_t length = (size_t)(value - start);
        if (!length || (length == 1 && start[0] == '.'))
            continue;
        if (length == 2 && start[0] == '.' && start[1] == '.')
            return false;
        if (used)
            relative[used++] = '/';
        memcpy(relative + used, start, length);
        used += length;
    }
    relative[used] = 0;
    return path_join(output, root, relative) && path_is_within_root(output, root);
}

bool path_preferences_file(char output[PATH_CAP], const char *root, const char *filename)
{
    char directory[PATH_CAP];
    return path_join(directory, root, FRONTEND_DATA_DIRECTORY) &&
           path_join(output, directory, filename);
}

static bool ensure_directory(const char *directory)
{
    if (mkdir(directory, 0777) == 0)
        return true;
    struct stat status;
    return errno == EEXIST && stat(directory, &status) == 0 && S_ISDIR(status.st_mode);
}

bool path_prepare_preferences_directory(const char *root)
{
    char directory[PATH_CAP];
    if (!path_join(directory, root, "_nds") || !ensure_directory(directory))
        return false;
    return path_join(directory, root, FRONTEND_DATA_DIRECTORY) && ensure_directory(directory);
}
