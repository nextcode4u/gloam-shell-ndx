#include "entry_sort.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    uint32_t before;
    Entry entries[ENTRY_CAP];
    uint32_t after;
} actual;
static Entry expected[ENTRY_CAP];

static int by_name(const void *a, const void *b)
{
    const Entry *left = a, *right = b;
    return strcmp(left->name, right->name);
}
static int by_date(const void *a, const void *b)
{
    const Entry *left = a, *right = b;
    return left->modified > right->modified ? -1 : left->modified < right->modified ? 1 : 0;
}
static int equal(const void *a, const void *b)
{
    (void)a;
    (void)b;
    return 0;
}
int main(void)
{
    const unsigned lengths[] = {0, 1, 2, 3, 7, 16, 17, 33, 127, 256, 511, ENTRY_CAP};
    for (unsigned trial = 0; trial < 24; trial++) {
        for (unsigned length = 0; length < sizeof lengths / sizeof *lengths; length++) {
            unsigned count = lengths[length];
            memset(&actual, 0, sizeof actual);
            actual.before = 0xA1308291;
            actual.after = 0xDE908273;
            for (unsigned i = 0; i < ENTRY_CAP; i++) {
                Entry *entry = actual.entries + i;
                snprintf(entry->name, sizeof entry->name, "Library entry %03u.nds", i);
                snprintf(entry->path, sizeof entry->path, "/roms/collection-%u/%03u.nds", i % 7, i);
                snprintf(entry->internal_title, sizeof entry->internal_title, "Internal %03u", i);
                entry->kind = i % 3 ? ITEM_FILE : ITEM_DIRECTORY;
                entry->size = i * 641;
                entry->modified = -(int64_t)i * 571;
                entry->target = i ^ 0x5A128;
                entry->title_loaded = (i & 1) != 0;
            }
            uint32_t random = 0x714A2B + trial;
            for (unsigned i = count; i > 1; i--) {
                random = random * 1664525U + 1013904223U;
                unsigned other = random % i;
                Entry saved = actual.entries[i - 1];
                actual.entries[i - 1] = actual.entries[other];
                actual.entries[other] = saved;
            }
            memcpy(expected, actual.entries, sizeof expected);
            int (*compare)(const void *, const void *) = trial & 1 ? by_date : by_name;
            qsort(expected, count, sizeof *expected, compare);
            entry_sort(actual.entries, count, compare);
            /* Independent libc reference checks every path/metadata byte,
             * including untouched records beyond this directory's count. */
            assert(!memcmp(actual.entries, expected, sizeof expected));
            assert(actual.before == 0xA1308291 && actual.after == 0xDE908273);
            entry_sort(actual.entries, count, equal);
            assert(!memcmp(actual.entries, expected, sizeof expected)); /* Stable ties. */
            entry_sort(actual.entries, ENTRY_CAP + 1, compare);
            assert(!memcmp(actual.entries, expected, sizeof expected));
        }
    }
    entry_sort(NULL, 2, by_name);
    entry_sort(actual.entries, 2, NULL);
    puts("PASS: indexed directory sorting matches libc across bounded permutations, preserves "
         "complete records, stable ties and buffer guards");
}
