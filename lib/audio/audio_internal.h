#ifndef AUDIO_INTERNAL_H
#define AUDIO_INTERNAL_H

#include <mad.h>
#include <pspkernel.h>
#include <stdbool.h>
#include "audio/audio.h"
#include "util/util.h"

typedef struct {
    char  *paths[AUDIO_MAX_TRACKS];
    int    count;
    int    current;
    repeat_mode_t repeat;
    bool   shuffle;
    SceUID lock;
} playlist_t;
// why de fuck ru here ok ill move it later TODO
typedef struct {
    short  data[AUDIO_RINGBUF_FRAMES * AUDIO_CHANNELS];
    volatile int head;
    volatile int tail;
    SceUID lock;
} ring_buffer_t;

extern playlist_t             g_pl;
extern ring_buffer_t          g_ring;
extern volatile audio_state_t g_state;
extern volatile bool          g_running;
extern int                    g_audio_channel;
extern volatile int           g_total_seconds;
extern volatile int           g_output_frames;

// ---- track-switch mailbox ----
// ONLY decode_thread ever calls decoder_open/decoder_close.
// Every other thread posts a request here instead of touching the decoder directly.
typedef enum {
    SWITCH_NONE,
    SWITCH_TO_PATH,   // load g_switch_path and play
    SWITCH_STOP,      // close whatever's open, go idle
} switch_kind_t;

extern volatile switch_kind_t g_switch_kind;
extern char                   g_switch_path[MAX_PATH];
extern SceUID                 g_switch_lock;

#endif
