#include <string.h>
#include "util/logging.h"
#include <pspaudio.h>
#include "audio/output_thread.h"
#include "audio/audio_internal.h"
#include "util/ringbuf.h"
#include <pspaudio_kernel.h>

int output_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    static bool logged_src = false, logged_hw = false;
    short out_chunk[AUDIO_CHUNK_FRAMES * AUDIO_CHANNELS] __attribute__((aligned(64)));
    while (g_running) {
        if (g_pending_rate != 0) {
            g_sample_rate = g_pending_rate;
            g_pending_rate = 0;
        }
        int n = ring_pop(&g_ring, out_chunk, AUDIO_CHUNK_FRAMES);
        if (n < AUDIO_CHUNK_FRAMES) {
  //   LOG_DEBUG(
  //     "output",
  //     "UNDERRUN: got %d/%d frames, ring now %d",
  //     n,
  //     AUDIO_CHUNK_FRAMES,
  //     ring_frames_available(&g_ring)
  // );
            memset(out_chunk + n * AUDIO_CHANNELS, 0,
                   (AUDIO_CHUNK_FRAMES - n) * AUDIO_CHANNELS * sizeof(short));
        }
        if (g_sample_rate == 48000 && g_src_reserved) {
            if (!logged_src) {
                logged_src = true;
                LOG_INFO("output", "SRC 48kHz output active");
            }
            sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, out_chunk);
        } else {
            if (!logged_hw) {
                logged_hw = true;
                LOG_INFO("output", "hardware 44.1kHz output active");
            }
            sceAudioOutputBlocking(g_audio_channel, PSP_AUDIO_VOLUME_MAX,
                                   out_chunk);
        }
        if (g_state == AUDIO_STATE_PLAYING)
            g_output_frames += AUDIO_CHUNK_FRAMES;

    }
    return 0;
}
