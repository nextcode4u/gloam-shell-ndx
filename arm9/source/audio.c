/* Read bounded PCM assets before submitting them to the hardware-facing layer. */
#include "audio.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t le32(const unsigned char *p)
{
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static unsigned le16(const unsigned char *p)
{
    return p[0] | ((unsigned)p[1] << 8);
}
void audio_free_clip(AudioClip *clip)
{
    free(clip->samples);
    memset(clip, 0, sizeof *clip);
}
bool audio_read_wav(AudioClip *clip, const char *path, size_t limit)
{
    if (clip->samples)
        return false;
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char header[16];
    bool valid = !fseek(file, 0, SEEK_END);
    long length = valid ? ftell(file) : -1;
    valid = length >= 44 && length <= (long)limit + 65536 && !fseek(file, 0, SEEK_SET) &&
            fread(header, 1, 12, file) == 12 && !memcmp(header, "RIFF", 4) &&
            !memcmp(header + 8, "WAVE", 4);
    uint32_t end = valid ? le32(header + 4) + 8 : 0;
    valid = valid && end >= 12 && end <= (uint32_t)length;
    bool format = false;
    unsigned rate = 0, data_size = 0;
    long data_start = 0;
    unsigned chunks = 0;
    for (uint32_t offset = 12; valid && offset + 8 <= end;) {
        if (++chunks > 128) {
            valid = false;
            break;
        }
        valid = !fseek(file, offset, SEEK_SET) && fread(header, 1, 8, file) == 8;
        uint32_t size = le32(header + 4);
        if (!valid || size > end - offset - 8) {
            valid = false;
            break;
        }
        if (!memcmp(header, "fmt ", 4)) {
            if (size < 16 || fread(header, 1, 16, file) != 16) {
                valid = false;
                break;
            }
            rate = le32(header + 4);
            format = le16(header) == 1 && le16(header + 2) == 1 && rate >= 8000 && rate <= 48000 &&
                     le16(header + 12) == 2 && le16(header + 14) == 16 &&
                     le32(header + 8) == rate * 2;
        } else if (!memcmp(header, "data", 4)) {
            data_start = offset + 8;
            data_size = size;
        }
        uint32_t next = offset + 8 + size;
        if (size & 1) {
            if (next == UINT32_MAX) {
                valid = false;
                break;
            }
            next++;
        }
        if (next > end) {
            valid = false;
            break;
        }
        offset = next;
    }
    AudioClip candidate = {0};
    valid = valid && format && data_size >= 4 && !(data_size & 1) && data_size <= limit;
    if (valid) {
        /* Sound DMA length uses whole words; pad rather than reading beyond the allocation. */
        candidate.bytes = (data_size + 3) & ~3u;
        candidate.samples = aligned_alloc(32, (candidate.bytes + 31) & ~31u);
        valid = candidate.samples && !fseek(file, data_start, SEEK_SET);
        if (valid) {
            memset(candidate.samples, 0, candidate.bytes);
            valid = fread(candidate.samples, 1, data_size, file) == data_size;
            candidate.rate = rate;
        }
    }
    fclose(file);
    if (!valid) {
        audio_free_clip(&candidate);
        return false;
    }
    *clip = candidate;
    return true;
}
void audio_request_reload(App *app)
{
    app->audio.reload_requested = true;
}
void audio_reload(App *app)
{
    AudioState *audio = &app->audio;
    audio_free_clip(&audio->music);
    for (int i = 0; i < 3; i++)
        audio_free_clip(&audio->effects[i]);
    audio->reload_requested = false;
    audio->status[0] = 0;
    audio->event = AUDIO_NONE;
    if (app->recovery_mode || !app->storage_ready) {
        path_copy(audio->status, "Audio disabled in recovery or without SD", sizeof audio->status);
        return;
    }
    const char *music = audio->mode == MUSIC_CUSTOM ? audio->custom_path : app->themes.active.music;
    if (audio->mode != MUSIC_OFF && *music &&
        !audio_read_wav(&audio->music, music, AUDIO_MUSIC_CAP))
        path_copy(audio->status, "Music unavailable: mono PCM16 WAV, max 1 MiB",
                  sizeof audio->status);
    else if (audio->mode != MUSIC_OFF && !*music)
        path_copy(audio->status, "No music track assigned", sizeof audio->status);
    if (audio->effects_enabled)
        for (int i = 0; i < 3; i++)
            if (*app->themes.active.sounds[i] &&
                !audio_read_wav(&audio->effects[i], app->themes.active.sounds[i], AUDIO_EFFECT_CAP))
                path_copy(audio->status, "One theme sound is missing or unsupported",
                          sizeof audio->status);
}
void audio_event(App *app, AudioEvent event)
{
    if (!app->recovery_mode && app->audio.effects_enabled)
        app->audio.event = event;
}
