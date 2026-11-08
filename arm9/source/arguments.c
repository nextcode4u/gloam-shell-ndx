/* Shared bounded shortcut parser for launching and icon metadata. */
#include "arguments.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool arguments_add(ArgumentList *args, const char *value)
{
    size_t length = strlen(value) + 1;
    if (args->count >= LAUNCH_ARG_CAP || length > LAUNCH_DATA_CAP - (size_t)args->used)
        return false;
    args->offsets[args->count++] = args->used;
    memcpy(args->data + args->used, value, length);
    args->used += (int)length;
    return true;
}
const char *arguments_get(const ArgumentList *args, int index)
{
    return index >= 0 && index < args->count ? args->data + args->offsets[index] : "";
}
bool arguments_read(const char *path, ArgumentList *args, char error[160])
{
    memset(args, 0, sizeof *args);
    FILE *file = fopen(path, "rb");
    if (!file) {
        snprintf(error, 160, "Could not read argv shortcut.");
        return false;
    }
    long size = -1;
    if (!fseek(file, 0, SEEK_END))
        size = ftell(file);
    if (size < 0 || size > LAUNCH_DATA_CAP || fseek(file, 0, SEEK_SET)) {
        fclose(file);
        snprintf(error, 160, "Shortcut exceeds the 4 KiB limit.");
        return false;
    }
    char *input = malloc((size_t)size + 1);
    if (!input) {
        fclose(file);
        snprintf(error, 160, "Not enough memory for shortcut.");
        return false;
    }
    bool valid = fread(input, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    input[size] = 0;
    if (strlen(input) != (size_t)size)
        valid = false;
    char token[LAUNCH_DATA_CAP];
    size_t used = 0;
    bool started = false;
    char quote = 0;
    for (char *p = input; valid && *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!quote && !started && (c == '#' || c == ';')) {
            while (*p && *p != '\n')
                p++;
            if (!*p)
                break;
            continue;
        }
        if (!quote && isspace(c)) {
            if (started) {
                token[used] = 0;
                valid = arguments_add(args, token);
                used = 0;
                started = false;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            if (!quote) {
                quote = (char)c;
                started = true;
                continue;
            }
            if (quote == (char)c) {
                quote = 0;
                continue;
            }
        }
        if (quote && c == '\\' && p[1] && (p[1] == quote || p[1] == '\\'))
            c = (unsigned char)*++p;
        started = true;
        if (used + 1 >= sizeof token) {
            valid = false;
            break;
        }
        token[used++] = (char)c;
    }
    if (quote)
        valid = false;
    if (valid && started) {
        token[used] = 0;
        valid = arguments_add(args, token);
    }
    free(input);
    if (!valid || !args->count || !*arguments_get(args, 0)) {
        snprintf(error, 160,
                 "Invalid shortcut quoting, empty application or argument limit exceeded.");
        return false;
    }
    return true;
}
