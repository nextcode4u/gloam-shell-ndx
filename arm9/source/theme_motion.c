/* One bounded accent, loaded only after the browser is usable. */
#include "theme_motion.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_BYTES (32 * 32 * 2 + 256)
static struct {
    char path[PATH_CAP];
    unsigned char *frames;
    unsigned count, phase, current;
    bool attempted;
} motion;

void themes_motion_reset(void)
{
    free(motion.frames);
    memset(&motion, 0, sizeof motion);
}

void themes_motion_tick(const App *app)
{
    const ThemePackage *theme = &app->themes.active;
    if (strcmp(motion.path, theme->ambient_path)) {
        themes_motion_reset();
        path_copy(motion.path, theme->ambient_path, sizeof motion.path);
    }
    if (!*motion.path || app->recovery_mode || !app->storage_ready || app->storage_lost ||
        app->themes.open || app->settings.open || app->picker.open || app->panel != PANEL_NONE ||
        app->scan_directory || app->frame_count < 90 || app->selection_idle_frames < 30)
        return;
    if (!motion.attempted) {
        motion.attempted = true;
        FILE *file = fopen(motion.path, "rb");
        if (!file)
            return;
        unsigned char header[16];
        bool valid = fread(header, 1, sizeof header, file) == sizeof header &&
                     !memcmp(header, "GLMFX001", 8) && header[8] >= 1 && header[8] <= 8 &&
                     header[9] == 32 && header[10] == 32;
        for (unsigned i = 11; valid && i < 16; i++)
            valid = header[i] == 0;
        size_t size = valid ? header[8] * FRAME_BYTES : 0;
        unsigned char *frames = valid ? malloc(size) : NULL;
        valid =
            frames && fread(frames, 1, size, file) == size && fgetc(file) == EOF && !ferror(file);
        fclose(file);
        if (valid) {
            motion.frames = frames;
            motion.count = header[8];
        } else
            free(frames);
    }
    if (!motion.frames)
        return;
    if (app->reduced_motion) {
        motion.current = motion.phase = 0;
        return;
    }
    unsigned ticks = theme->ambient_ticks;
    if (ticks < 5 || ticks > 120)
        ticks = 12;
    if (++motion.phase >= ticks) {
        motion.phase = 0;
        motion.current = (motion.current + 1) % motion.count;
    }
}

const uint16_t *themes_motion_frame(const App *app, const uint8_t **coverage)
{
    *coverage = NULL;
    if (!motion.frames || strcmp(motion.path, app->themes.active.ambient_path) ||
        app->recovery_mode)
        return NULL;
    unsigned index = app->reduced_motion ? 0 : motion.current;
    const unsigned char *frame = motion.frames + index * FRAME_BYTES;
    *coverage = frame + 2048;
    return (const uint16_t *)frame;
}
