#ifndef FRONTEND_TEXT_H
#define FRONTEND_TEXT_H
#include <stddef.h>
#include <stdint.h>
/* Invalid UTF-8 consumes one byte and yields '?'; valid unsupported glyphs
 * remain one cell. No operation splits a multi-byte character. */
uint32_t text_next(const char **cursor);
size_t text_prefix_bytes(const char *text, size_t characters);
size_t text_length(const char *text);
void text_from_utf16le(char *output, size_t capacity, const unsigned char *data, size_t units);
#endif
