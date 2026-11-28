#ifndef FRONTEND_ITEM_PREFERENCES_H
#define FRONTEND_ITEM_PREFERENCES_H
#include "types.h"
typedef struct {
    char cover[PATH_CAP];
    char icon[PATH_CAP];
    char application[PATH_CAP];
} ItemPreferences;
bool item_preferences_load(const char *root, const char *content, ItemPreferences *item);
/* Small presentation-only cache; launch/edit checks always read current records. */
bool item_preferences_display(const char *root, const char *content, ItemPreferences *item);
void item_preferences_display_clear(void);
unsigned item_preferences_display_generation(void);
/* Launch checks allow missing records, but reject unrecovered read errors.
 * Failure leaves the caller's record unchanged. Display fallbacks use load(). */
bool item_preferences_resolve(const char *root, const char *content, ItemPreferences *item);
/* Editing must inspect the primary; only a fully read damaged record may use backup.
 * Missing records are empty; failure leaves the caller's record unchanged. */
bool item_preferences_read_for_edit(const char *root, const char *content, ItemPreferences *item);
bool item_preferences_save(const char *root, const char *content, const ItemPreferences *item);
#endif
