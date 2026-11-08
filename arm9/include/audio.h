#ifndef FRONTEND_AUDIO_H
#define FRONTEND_AUDIO_H
#include "types.h"
#define AUDIO_MUSIC_CAP (1024 * 1024)
#define AUDIO_EFFECT_CAP (64 * 1024)
bool audio_read_wav(AudioClip *clip, const char *path, size_t limit);
void audio_free_clip(AudioClip *clip);
void audio_reload(App *app); /* Caller must stop hardware channels first. */
void audio_request_reload(App *app);
void audio_event(App *app, AudioEvent event);
#endif
