/* Sort directory records through small indexes, avoiding repeated large swaps. */
#ifndef FRONTEND_ENTRY_SORT_H
#define FRONTEND_ENTRY_SORT_H
#include "types.h"
void entry_sort(Entry *entries, unsigned count,
                int (*compare)(const void *left, const void *right));
#endif
