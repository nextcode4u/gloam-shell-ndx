/* Bounded PNG/JPEG decoding with optional cooperative progress checkpoints. */
#include "image_job.h"
#define STBI_JPEG_PROGRESS() image_decode_checkpoint()
#define STBI_JPEG_CANCELLED() image_decode_cancelled()
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_SUPPORT_ZLIB
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_MAX_DIMENSIONS 512
#include "image_decode.h"
#include "lodepng.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "stb_image.h"
#pragma GCC diagnostic pop
#include <stdlib.h>
#include <string.h>

/* Fixed-output zlib decoding for prepared splash frames. stb never grows this
 * buffer; verify the stream's Adler checksum as well as its exact output size. */
bool image_decode_zlib_frame(unsigned char *out, size_t size, const unsigned char *data,
                             size_t bytes)
{
    if (!out || !data || !size || size > 256u * 192 * 2 || bytes < 6 || bytes > size + 1024)
        return false;
    if (stbi_zlib_decode_buffer((char *)out, (int)size, (const char *)data, (int)bytes) !=
        (int)size)
        return false;
    unsigned a = 1, b = 0;
    for (size_t start = 0; start < size;) {
        size_t end = start + 5552 < size ? start + 5552 : size;
        while (start < end) {
            a += out[start++];
            b += a;
        }
        a %= 65521;
        b %= 65521;
    }
    const unsigned char *sum = data + bytes - 4;
    uint32_t expected =
        (uint32_t)sum[0] << 24 | (uint32_t)sum[1] << 16 | (uint32_t)sum[2] << 8 | sum[3];
    return (b << 16 | a) == expected;
}

static uint16_t rgb(unsigned r, unsigned g, unsigned b)
{
    return 0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);
}
static unsigned char *decode_rgba(const unsigned char *data, size_t size, unsigned *out_width,
                                  unsigned *out_height)
{
    if (!data || size < 24 || size > 1024 * 1024)
        return NULL;
    unsigned char *rgba = NULL;
    unsigned width = 0, height = 0;
    if (!memcmp(data, "\x89PNG\r\n\x1a\n", 8)) {
        width = ((unsigned)data[16] << 24) | ((unsigned)data[17] << 16) |
                ((unsigned)data[18] << 8) | data[19];
        height = ((unsigned)data[20] << 24) | ((unsigned)data[21] << 16) |
                 ((unsigned)data[22] << 8) | data[23];
        if (!width || !height || width > 512 || height > 512 ||
            lodepng_decode32(&rgba, &width, &height, data, size)) {
            free(rgba);
            return NULL;
        }
    } else if (data[0] == 0xFF && data[1] == 0xD8) {
        int w, h, channels;
        if (!stbi_info_from_memory(data, (int)size, &w, &h, &channels) || w < 1 || h < 1 ||
            w > 512 || h > 512)
            return NULL;
        rgba = stbi_load_from_memory(data, (int)size, &w, &h, &channels, 4);
        if (!rgba)
            return NULL;
        width = (unsigned)w;
        height = (unsigned)h;
    } else
        return NULL;
    *out_width = width;
    *out_height = height;
    return rgba;
}
bool image_decode_icon(uint16_t pixels[32 * 32], const unsigned char *data, size_t size)
{
    return image_decode_component(pixels, NULL, data, size);
}

bool image_decode_component(uint16_t pixels[32 * 32], uint8_t coverage[256],
                            const unsigned char *data, size_t size)
{
    static const unsigned char dimensions[] = {0, 0, 0, 32, 0, 0, 0, 32};
    if (!data || size < 24 || size > 65536 || memcmp(data, "\x89PNG\r\n\x1a\n", 8) ||
        memcmp(data + 16, dimensions, sizeof dimensions))
        return false;
    unsigned width, height;
    unsigned char *rgba = decode_rgba(data, size, &width, &height);
    if (!rgba)
        return false;
    if (coverage)
        memset(coverage, 0, 256);
    for (int i = 0; i < 32 * 32; i++) {
        unsigned char *pixel = rgba + i * 4;
        unsigned alpha = coverage ? (pixel[3] * 3u + 127) / 255 : pixel[3] < 128 ? 0 : 3;
        pixels[i] = alpha ? rgb(pixel[0], pixel[1], pixel[2]) : 0;
        if (coverage)
            coverage[i / 4] |= alpha << ((i % 4) * 2);
    }
    stbi_image_free(rgba);
    return true;
}

bool image_decode(Artwork *artwork, const unsigned char *data, size_t size)
{
    unsigned width, height;
    unsigned char *rgba = decode_rgba(data, size, &width, &height);
    if (!rgba || image_job_cancelled()) {
        stbi_image_free(rgba);
        return false;
    }
    int scaled_width = 96, scaled_height = (int)(height * 96 / width);
    if (scaled_height > 104) {
        scaled_height = 104;
        scaled_width = (int)(width * 104 / height);
    }
    if (scaled_width < 1)
        scaled_width = 1;
    if (scaled_height < 1)
        scaled_height = 1;
    for (int y = 0; y < scaled_height; y++) {
        image_decode_checkpoint();
        if (image_job_cancelled()) {
            stbi_image_free(rgba);
            return false;
        }
        for (int x = 0; x < scaled_width; x++) {
            const unsigned char *pixel =
                rgba + 4 * ((y * height / scaled_height) * width + x * width / scaled_width);
            unsigned a = pixel[3];
            artwork->pixels[y * scaled_width + x] =
                rgb((pixel[0] * a + 19 * (255 - a)) / 255, (pixel[1] * a + 24 * (255 - a)) / 255,
                    (pixel[2] * a + 34 * (255 - a)) / 255);
        }
    }
    stbi_image_free(rgba); /* Both configured decoders use malloc/free. */
    artwork->width = scaled_width;
    artwork->height = scaled_height;
    artwork->ready = true;
    return true;
}

bool image_decode_background(uint16_t pixels[256 * 192], const unsigned char *data, size_t size,
                             uint16_t matte, unsigned strength)
{
    if (!pixels || strength > 100)
        return false;
    unsigned width, height;
    unsigned char *rgba = decode_rgba(data, size, &width, &height);
    if (!rgba)
        return false;
    unsigned crop_width = width, crop_height = height;
    if (width * 192 > height * 256)
        crop_width = height * 256 / 192;
    else
        crop_height = width * 192 / 256;
    if (!crop_width)
        crop_width = 1;
    if (!crop_height)
        crop_height = 1;
    unsigned origin_x = (width - crop_width) / 2, origin_y = (height - crop_height) / 2;
    unsigned bg[3] = {(matte & 31) * 255 / 31, ((matte >> 5) & 31) * 255 / 31,
                      ((matte >> 10) & 31) * 255 / 31};
    for (unsigned y = 0; y < 192; y++)
        for (unsigned x = 0; x < 256; x++) {
            const unsigned char *pixel = rgba + 4 * ((origin_y + y * crop_height / 192) * width +
                                                     origin_x + x * crop_width / 256);
            unsigned alpha = pixel[3] * strength;
            unsigned r = (pixel[0] * alpha + bg[0] * (25500 - alpha)) / 25500;
            unsigned g = (pixel[1] * alpha + bg[1] * (25500 - alpha)) / 25500;
            unsigned b = (pixel[2] * alpha + bg[2] * (25500 - alpha)) / 25500;
            pixels[y * 256 + x] = rgb(r, g, b);
        }
    stbi_image_free(rgba);
    return true;
}
