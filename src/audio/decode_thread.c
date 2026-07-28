#include <string.h>
#include <pspiofilemgr.h>
#include <mad.h>
#include "audio/decode_thread.h"
#include "audio/audio_internal.h"
#include "util/ringbuf.h"
#include "core/playlist.h"
#include "util/logging.h"

#define TAG "decode_thread"

static struct mad_stream g_stream;
static struct mad_frame  g_frame;
static struct mad_synth  g_synth;
static SceUID            g_fd = -1;
static bool               g_decoder_open = false;


static void decoder_close(void) {
    if (!g_decoder_open) return;
    mad_synth_finish(&g_synth);
    mad_frame_finish(&g_frame);
    mad_stream_finish(&g_stream);
    if (g_fd >= 0) { sceIoClose(g_fd); g_fd = -1; }
    g_decoder_open = false;
}

static bool decoder_fill(void) {
    static unsigned char in_buf[8192 + MAD_BUFFER_GUARD];
    size_t remaining = g_stream.bufend - g_stream.next_frame;
    memmove(in_buf, g_stream.next_frame, remaining);

    int n = sceIoRead(g_fd, in_buf + remaining, 8192 - remaining); // note: cap to 8192, not sizeof(in_buf)
    if (n <= 0 && remaining == 0) return false;

    size_t valid = remaining + (n > 0 ? n : 0);
    memset(in_buf + valid, 0, MAD_BUFFER_GUARD); // guarantee guard bytes are zero, every call

    mad_stream_buffer(&g_stream, in_buf, valid);
    return true;
}

static bool decoder_open(const char *path) {
    LOG_DEBUG(TAG, "opening: %s", path);
    g_fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (g_fd < 0) {
        LOG_ERR(TAG, "sceIoOpen failed: %d", g_fd);
        return false;
    }

    mad_stream_init(&g_stream);
    // ^ dis zeros the struct which the mad_stream_buffer needs so initial run kaboom
    mad_frame_init(&g_frame);
    mad_synth_init(&g_synth);
    if (!decoder_fill()) {
        LOG_ERR(TAG, "decoder_fill failed on open (empty file?)");
        decoder_close();
        return false;
    }
    g_decoder_open = true;
    return true;
}

static void handle_pending_switch(void) {
    switch_kind_t kind;
    char path[MAX_PATH];

    sceKernelWaitSema(g_switch_lock, 1, NULL);
    kind = g_switch_kind;
    if (kind == SWITCH_TO_PATH) {
        strncpy(path, g_switch_path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }
    g_switch_kind = SWITCH_NONE;
    sceKernelSignalSema(g_switch_lock, 1);

    if (kind == SWITCH_NONE) return;
    LOG_DEBUG(TAG, "handling switch, path len=%d", (int)strlen(path));

    decoder_close();

    if (kind == SWITCH_STOP) {
        g_state = AUDIO_STATE_STOPPED;
        return;
    }

    // kind == SWITCH_TO_PATH
    if (!decoder_open(path)) {
        g_state = AUDIO_STATE_STOPPED;
    } else {
        g_state = AUDIO_STATE_PLAYING;
    }
}

int decode_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    short pcm_chunk[AUDIO_CHUNK_FRAMES * AUDIO_CHANNELS];

    while (g_running) {
        handle_pending_switch();

        if (g_state != AUDIO_STATE_PLAYING || !g_decoder_open) {
            sceKernelDelayThread(10 * 1000);
            continue;
        }

        if (ring_frames_free(&g_ring) < AUDIO_CHUNK_FRAMES) {
            sceKernelDelayThread(5 * 1000);
            continue;
        }

        if (mad_frame_decode(&g_frame, &g_stream) == -1) {
            if (g_stream.error == MAD_ERROR_BUFLEN) {
                if (!decoder_fill()) {
                    // real EOF: ask playlist what's next, ourselves — no cross-thread call needed
                    const char *next = playlist_advance(&g_pl);
                    decoder_close();
                    if (!next || !decoder_open(next)) {
                        g_state = AUDIO_STATE_STOPPED;
                    }
                }
                continue;
            }
            continue; // recoverable decode error, skip frame
        }

        mad_synth_frame(&g_synth, &g_frame);
        int nframes = g_synth.pcm.length;
        for (int i = 0; i < nframes && i < AUDIO_CHUNK_FRAMES; i++) {
            pcm_chunk[i * 2]     = (short)(mad_f_todouble(g_synth.pcm.samples[0][i]) * 32767);
            pcm_chunk[i * 2 + 1] = (short)(mad_f_todouble(g_synth.pcm.samples[1][i]) * 32767);
        }
        ring_push(&g_ring, pcm_chunk, nframes);
    }

    decoder_close();
    return 0;
}
