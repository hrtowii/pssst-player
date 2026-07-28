#include <string.h>
#include <pspiofilemgr.h>
#include "audio/decode_thread.h"
#include "audio/audio_internal.h"
#include "audio/decoder.h"
#include "util/ringbuf.h"
#include "player/playlist.h"
#include "util/logging.h"

#define TAG "decode_thread"

static decoder_t       g_decoder;
static bool            g_decoder_open = false;


static void decoder_close(void) {
    if (!g_decoder_open) return;
    g_decoder.close(&g_decoder);
    g_decoder_open = false;
    g_output_frames = 0;
}

static bool decoder_open(const char *path) {
    if (g_decoder_open) decoder_close();
// TODO: add libflac
    memcpy(&g_decoder, &decoder_mp3, sizeof(g_decoder));
    if (!g_decoder.open(&g_decoder, path)) {
        LOG_ERR(TAG, "decoder_open failed: %s", path);
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
            sceKernelDelayThread(10 * 100);
            continue;
        }

        if (ring_frames_free(&g_ring) < AUDIO_CHUNK_FRAMES) {
            sceKernelDelayThread(5 * 100);
            continue;
        }

        int n = g_decoder.read_pcm(&g_decoder, pcm_chunk, AUDIO_CHUNK_FRAMES);
        if (n <= 0) {
            // EOF — advance to next track
            const char *next = playlist_advance(&g_pl);
            decoder_close();
            if (!next || !decoder_open(next)) {
                g_state = AUDIO_STATE_STOPPED;
            }
            continue;
        }

        ring_push(&g_ring, pcm_chunk, n);
    }

    decoder_close();
    return 0;
}
