#ifndef FRONTEND_SPLASH_H
#define FRONTEND_SPLASH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPLASH_FILE_CAP (2u * 1024 * 1024)
#define SPLASH_LAYER_CAP 4
#define SPLASH_TICK_CAP 600
typedef enum { SPLASH_OFF, SPLASH_THEME, SPLASH_CUSTOM, SPLASH_MODE_COUNT } SplashMode;
typedef struct {
    unsigned screen, format, effect, width, height, frames, hold, delay, transition;
    int x, y;
    bool motion;
    int start_x, start_y, end_x, end_y;
    unsigned start_scale, end_scale, start_opacity, end_opacity;
    unsigned easing, repeat, sequence;
    const unsigned char *palette, *pixels;
    const unsigned char *frame_table;
    unsigned char *decoded;
    unsigned decoded_frame;
} SplashLayer;
typedef struct {
    unsigned char *data;
    size_t bytes;
    unsigned duration, poster, count;
    uint16_t background;
    SplashLayer layers[SPLASH_LAYER_CAP];
    const int16_t *sound;
    unsigned sound_bytes;
} Splash;

/* A failed load leaves an empty object. Initialize it to zero before first use.
 * Stop hardware audio before closing a splash: samples belong to this object. */
bool splash_load(Splash *splash, const char *path);
void splash_close(Splash *splash);
/* Decode only changed compressed frames into buffers allocated at load time.
 * No I/O or allocations here. A failed decode must end playback. */
bool splash_prepare(Splash *splash, unsigned ticks, bool reduced_motion);
/* No allocation, file I/O or hardware access while rendering prepared assets. */
void splash_render(const Splash *splash, unsigned ticks, bool reduced_motion,
                   uint16_t top[256 * 192], uint16_t bottom[256 * 192]);
bool splash_playing(const Splash *splash, unsigned ticks, bool loading, bool play_once,
                    bool skipped);
bool splash_needs_frame(const Splash *splash, unsigned previous, unsigned ticks,
                        bool reduced_motion);
#endif
