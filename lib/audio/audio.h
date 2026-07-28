#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stddef.h>

#define AUDIO_MAX_TRACKS   512
#define AUDIO_SAMPLE_RATE  44100
#define AUDIO_CHANNELS     2
#define AUDIO_CHUNK_FRAMES 2048
#define AUDIO_RINGBUF_FRAMES (AUDIO_SAMPLE_RATE)

typedef enum {
    AUDIO_STATE_STOPPED,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED,
} audio_state_t;

typedef enum {
    REPEAT_OFF,
    REPEAT_ONE,
    REPEAT_ALL,
} repeat_mode_t;

bool audio_init(void);
void audio_shutdown(void);

bool audio_playlist_set(const char **paths, int count);
bool audio_playlist_add(const char *path);
void audio_playlist_clear(void);

void audio_play_index(int index);
void audio_play(void);
void audio_pause(void);
void audio_stop(void);
void audio_next(void);
void audio_prev(void);

void audio_set_repeat(repeat_mode_t mode);
void audio_set_shuffle(bool on);

audio_state_t audio_get_state(void);
int           audio_get_current_index(void);
const char   *audio_get_current_path(void);

#endif
