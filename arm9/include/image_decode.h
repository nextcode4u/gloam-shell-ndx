#ifndef FRONTEND_IMAGE_DECODE_H
#define FRONTEND_IMAGE_DECODE_H
#include "types.h"
/* A full-screen PNG quantized to two four-bit shade indices per byte. */
bool image_decode_material(uint8_t shades[256 * 192 / 2], const unsigned char *data, size_t size);
bool image_decode_zlib(unsigned char *out, size_t size, const unsigned char *data, size_t bytes);
bool image_decode_zlib_frame(unsigned char *out, size_t size, const unsigned char *data,
                             size_t bytes);
bool image_decode_icon(uint16_t pixels[32 * 32], const unsigned char *data, size_t size);
bool image_decode_component(uint16_t pixels[32 * 32], uint8_t coverage[256],
                            const unsigned char *data, size_t size);
bool image_decode(Artwork *artwork, const unsigned char *data, size_t size);
bool image_decode_background(uint16_t pixels[256 * 192], const unsigned char *data, size_t size,
                             uint16_t matte, unsigned strength);
/* Exact-size RGBA PNG, composited once onto a prepared background. */
bool image_decode_surface(uint16_t pixels[256 * 192], const unsigned char *data, size_t size,
                          unsigned opacity);
bool embedded_album_art(Artwork *artwork, const char *path);
#endif
