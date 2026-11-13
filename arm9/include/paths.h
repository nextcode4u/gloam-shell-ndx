#ifndef FRONTEND_PATHS_H
#define FRONTEND_PATHS_H
#include "types.h"

#define FRONTEND_DATA_DIRECTORY "_nds/dsi-frontend"
#define FRONTEND_LEGACY_DIRECTORY "data/dsi-frontend"

void path_copy(char *destination, const char *source, size_t capacity);
bool path_join(char output[PATH_CAP], const char *directory, const char *filename);
const char *path_extension(const char *path);
bool path_is_within_root(const char *path, const char *root);
void path_default_category(char output[PATH_CAP], const char *root, int category);
bool path_resolve_sd(char output[PATH_CAP], const char *root, const char *value);
bool path_preferences_file(char output[PATH_CAP], const char *root, const char *filename);
bool path_prepare_preferences_directory(const char *root);
typedef enum {
    FILE_COMMIT_OK,
    FILE_COMMIT_INSPECT,
    FILE_COMMIT_REMOVE_BACKUP,
    FILE_COMMIT_BACKUP,
    FILE_COMMIT_INSTALL,
    FILE_COMMIT_RESTORE
} FileCommitResult;
/* Commit a fully written, closed temporary file using FAT-safe rename rules.
 * Preserve errno on failure and retain a lone backup across retries.
 * This is recoverable replacement, not power-loss atomicity. */
FileCommitResult path_commit_file(const char *temporary, const char *current, const char *backup);
#endif
