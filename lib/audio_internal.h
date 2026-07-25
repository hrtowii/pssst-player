#ifndef AUDIO_INTERNAL_H
#define AUDIO_INTERNAL_H

#include <mad.h>
#include <pspkernel.h>
#include <stdbool.h>
#include "audio.h"

typedef struct {
    char  *paths[AUDIO_MAX_TRACKS];
    int    count;
    int    current;
    repeat_mode_t repeat;
    bool   shuffle;
    SceUID lock;               // semaphore guarding this struct
} playlist_t;

typedef struct {
    short  data[AUDIO_RINGBUF_FRAMES * AUDIO_CHANNELS];
    volatile int head;         // write pos, frames
    volatile int tail;         // read pos, frames
    SceUID lock;
} ring_buffer_t;

// shared global state — declared here, defined in audio.c
extern playlist_t            g_pl;
extern ring_buffer_t         g_ring;
extern volatile audio_state_t g_state;
extern volatile bool          g_running;
extern int                    g_audio_channel;

#endif
