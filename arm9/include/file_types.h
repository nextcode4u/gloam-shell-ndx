#ifndef FRONTEND_FILE_TYPES_H
#define FRONTEND_FILE_TYPES_H
#include "types.h"

typedef enum {
    PLACEHOLDER_FILE,
    PLACEHOLDER_FOLDER,
    PLACEHOLDER_GAME,
    PLACEHOLDER_APPLICATION,
    PLACEHOLDER_AUDIO,
    PLACEHOLDER_VIDEO,
    PLACEHOLDER_BOOK,
    PLACEHOLDER_TEXT,
    PLACEHOLDER_IMAGE,
    PLACEHOLDER_SAVE,
    PLACEHOLDER_CONFIG,
    PLACEHOLDER_ARCHIVE,
    PLACEHOLDER_SHORTCUT,
    PLACEHOLDER_SYSTEM,
    PLACEHOLDER_MISSING,
    PLACEHOLDER_COUNT
} PlaceholderIcon;
typedef struct {
    PlaceholderIcon icon;
    const char *label;
} EntryFileType;
/* Presentation only: never opens files or changes associations/launch routes. */
EntryFileType entry_file_type(const Entry *entry);
const char *placeholder_label(PlaceholderIcon icon);
#endif
