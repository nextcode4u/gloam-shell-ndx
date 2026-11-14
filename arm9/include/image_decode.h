#ifndef FRONTEND_IMAGE_DECODE_H
#define FRONTEND_IMAGE_DECODE_H
#include "types.h"
bool image_decode_icon(uint16_t pixels[32 * 32], const unsigned char *data, size_t size);
bool image_decode_component(uint16_t pixels[32 * 32], uint8_t coverage[256],
                            const unsigned char *data, size_t size);
bool image_decode(Artwork *artwork, const unsigned char *data, size_t size);
bool image_decode_background(uint16_t pixels[256 * 192], const unsigned char *data, size_t size,
                             uint16_t matte, unsigned strength);
bool embedded_album_art(Artwork *artwork, const char *path);
#endif
