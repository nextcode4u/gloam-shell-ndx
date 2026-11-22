/* Shared bounded shortcut parser for launching and icon metadata. */
#include "arguments.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    /* Seeking to EOF can walk a large FAT cluster chain before rejecting it.
     * Validate the open file's metadata without moving its read position. */
    struct stat metadata;
    bool inspected =
        !fstat(fileno(file), &metadata) && S_ISREG(metadata.st_mode) && metadata.st_size >= 0;
    if (!inspected || metadata.st_size > LAUNCH_DATA_CAP) {
        fclose(file);
        snprintf(error, 160,
                 inspected ? "Shortcut exceeds the 4 KiB limit."
                           : "Could not inspect argv shortcut.");
        return false;
    }
    size_t size = (size_t)metadata.st_size;
    char *input = malloc(size + 1);
    if (!input) {
        fclose(file);
        snprintf(error, 160, "Not enough memory for shortcut.");
        return false;
    }
    bool valid = fread(input, 1, size, file) == size && !ferror(file);
    fclose(file);
    if (!valid) {
        free(input);
        snprintf(error, 160, "Could not finish reading argv shortcut.");
        return false;
    }
    input[size] = 0;
    if (strlen(input) != size)
        valid = false;
    char token[LAUNCH_DATA_CAP];
    size_t used = 0;
    bool started = false;
    char quote = 0;
    /* A UTF-8 BOM belongs to the file encoding, not argv[0] or a comment. */
    char *first = size >= 3 && !memcmp(input, "\xEF\xBB\xBF", 3) ? input + 3 : input;
    for (char *p = first; valid && *p; p++) {
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
