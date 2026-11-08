#ifndef FRONTEND_IMAGE_INPUT_H
#define FRONTEND_IMAGE_INPUT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
/* Filesystem calls finish before yielding. Cancellation closes the owning
 * read-only FILE in its caller without reading further source bytes. */
bool image_read_exact(FILE *file, void *destination, size_t bytes);
#endif
