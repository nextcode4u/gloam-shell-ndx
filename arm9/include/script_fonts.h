#ifndef GLOAM_SCRIPT_FONTS_H
#define GLOAM_SCRIPT_FONTS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Controller-owned SD reads. Drawing only consults the prepared glyph tables. */
void script_fonts_configure(const char *root, bool enabled);
void script_fonts_prepare_text(const char *text);
bool script_fonts_glyph(unsigned character, int row, unsigned *advance, uint32_t *coverage);
size_t script_fonts_bytes(void);
unsigned script_fonts_count(void);
unsigned script_fonts_attempted(void);
#endif
