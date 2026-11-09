/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scummvm.h"
#include "arguments.h"
#include "paths.h"
#include <ctype.h>
#include <stdio.h>
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

bool scummvm_prepare_arguments(const char *root, LaunchPlan *plan, char error[160])
{
    const char *name = strrchr(plan->application, '/');
    name = name ? name + 1 : plan->application;
    /* Explicit shortcuts/game IDs own their arguments. Other applications do no I/O. */
    if (plan->route != LAUNCH_DIRECT || plan->arguments.count != 1 ||
        strcasecmp(name, "scummvm.nds"))
        return true;

    char path[PATH_CAP], config[4097];
    if (!path_join(path, root, FRONTEND_DATA_DIRECTORY "/plugins/startup/scummvm.ini"))
        return true;
    FILE *file = fopen(path, "rb");
    if (!file)
        return true;
    size_t length = fread(config, 1, sizeof config, file);
    bool valid = !ferror(file) && length < sizeof config && !memchr(config, 0, length);
    fclose(file);
    if (!valid)
        goto invalid;
    config[length] = 0;

    char application[PATH_CAP] = "/roms/ScummVM/scummvm.nds";
    char titles[PATH_CAP] = "/roms/ScummVM/titles";
    bool section = false, found = false, auto_add = true, recursive = true;
    char *input = config, *next;
    if (length >= 3 && !memcmp(input, "\xEF\xBB\xBF", 3))
        input += 3;
    for (char *line = strtok_r(input, "\n", &next); line; line = strtok_r(NULL, "\n", &next)) {
        char *key = trim(line);
        if (!*key || *key == '#' || *key == ';')
            continue;
        if (*key == '[') {
            section = !strcasecmp(key, "[scummvm]");
            found |= section;
            continue;
        }
        if (!section)
            continue;
        char *value = strchr(key, '=');
        if (!value)
            goto invalid;
        *value++ = 0;
        key = trim(key);
        value = trim(value);
        if (!strcasecmp(key, "application") || !strcasecmp(key, "titles")) {
            if (!*value || strlen(value) >= PATH_CAP)
                goto invalid;
            path_copy(!strcasecmp(key, "application") ? application : titles, value, PATH_CAP);
        } else if (!strcasecmp(key, "auto_add") || !strcasecmp(key, "recursive")) {
            if (strcmp(value, "0") && strcmp(value, "1"))
                goto invalid;
            if (!strcasecmp(key, "auto_add"))
                auto_add = *value == '1';
            else
                recursive = *value == '1';
        }
    }
    if (!found)
        goto invalid;
    if (!auto_add)
        return true;
    if (!path_resolve_sd(path, root, application))
        goto invalid;
    if (strcasecmp(path, plan->application))
        return true;
    if (!path_resolve_sd(path, root, titles) || !path_is_within_root(path, root))
        goto invalid;
    struct stat info;
    /* A moved or not-yet-created collection must not prevent opening ScummVM itself. */
    if (stat(path, &info) || !S_ISDIR(info.st_mode))
        return true;
    char option[PATH_CAP + 8];
    snprintf(option, sizeof option, "--path=%s", path);
    if (!arguments_add(&plan->arguments, "--add") ||
        (recursive && !arguments_add(&plan->arguments, "--recursive")) ||
        !arguments_add(&plan->arguments, option) || !arguments_add(&plan->arguments, "--no-exit")) {
        snprintf(error, 160, "ScummVM discovery exceeds the launch argument limit.");
        return false;
    }
    return true;
invalid:
    snprintf(error, 160, "Invalid plugins/startup/scummvm.ini configuration.");
    return false;
}
