#ifndef FRONTEND_ITEM_PREFERENCES_H
#define FRONTEND_ITEM_PREFERENCES_H
#include "types.h"
typedef struct {
    char cover[PATH_CAP];
    char icon[PATH_CAP];
    char application[PATH_CAP];
} ItemPreferences;
bool item_preferences_load(const char *root, const char *content, ItemPreferences *item);
bool item_preferences_save(const char *root, const char *content, const ItemPreferences *item);
#endif
