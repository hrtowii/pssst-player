#include <string.h>
#include <pspaudio.h>
#include "audio/output_thread.h"
#include "audio/audio_internal.h"
#include "util/ringbuf.h"

int output_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    short out_chunk[AUDIO_CHUNK_FRAMES * AUDIO_CHANNELS];

    while (g_running) {
        int n = ring_pop(&g_ring, out_chunk, AUDIO_CHUNK_FRAMES);
        if (n < AUDIO_CHUNK_FRAMES) {
            memset(out_chunk + n * AUDIO_CHANNELS, 0,
                   (AUDIO_CHUNK_FRAMES - n) * AUDIO_CHANNELS * sizeof(short));
        }
        sceAudioOutputBlocking(g_audio_channel, PSP_AUDIO_VOLUME_MAX, out_chunk);
        if (g_state == AUDIO_STATE_PLAYING)
            g_output_frames += AUDIO_CHUNK_FRAMES;
    }
    return 0;
}
