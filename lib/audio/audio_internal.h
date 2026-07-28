#ifndef AUDIO_INTERNAL_H
#define AUDIO_INTERNAL_H

#include <pspkernel.h>
#include <stdbool.h>
#include "audio/audio.h"
#include "util/util.h"
#include "player/playlist.h"
#include "util/ringbuf.h"

extern playlist_t             g_pl;
extern ring_buffer_t          g_ring;
extern volatile audio_state_t g_state;
extern volatile bool          g_running;
extern int                    g_audio_channel;
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
