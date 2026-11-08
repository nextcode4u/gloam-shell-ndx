#ifndef FRONTEND_CONFIGURATION_H
#define FRONTEND_CONFIGURATION_H
#include "types.h"
/* Portable uncompressed DSCFG001 records; no executable/content files. */
bool configuration_export(const char *root, char error[160]);
bool configuration_inspect(const char *path, unsigned *count, char error[160]);
bool configuration_import(const char *root, const char *path, char error[160]);
bool configuration_recover(const char *root, char error[160]);
#endif
