#include "entry_sort.h"
#include <string.h>

void entry_sort(Entry *entries, unsigned count, int (*compare)(const void *left, const void *right))
{
    if (!entries || !compare || count < 2 || count > ENTRY_CAP)
        return;
    uint16_t order[ENTRY_CAP], merged[ENTRY_CAP];
    for (unsigned i = 0; i < count; i++)
        order[i] = i;
    /* Bottom-up merge sort needs no recursion or comparator-global context.
     * Only indexes move during comparisons; source records stay unchanged. */
    for (unsigned width = 1; width < count; width *= 2) {
        for (unsigned start = 0; start < count; start += width * 2) {
            unsigned middle = start + width < count ? start + width : count;
            unsigned end = middle + width < count ? middle + width : count;
            unsigned left = start, right = middle;
            for (unsigned out = start; out < end; out++) {
                if (left < middle &&
                    (right == end || compare(&entries[order[left]], &entries[order[right]]) <= 0))
                    merged[out] = order[left++];
                else
                    merged[out] = order[right++];
            }
        }
        memcpy(order, merged, count * sizeof *order);
    }
    /* Apply each permutation cycle once, keeping path, labels and metadata
     * together. Mark completed destinations with their own index. */
    for (unsigned start = 0; start < count; start++) {
        if (order[start] == start)
            continue;
        Entry saved = entries[start];
        unsigned destination = start;
        while (order[destination] != start) {
            unsigned source = order[destination];
            entries[destination] = entries[source];
            order[destination] = destination;
            destination = source;
        }
        entries[destination] = saved;
        order[destination] = destination;
    }
}
