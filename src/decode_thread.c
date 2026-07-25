#include <string.h>
#include <pspiofilemgr.h>
#include <mad.h>
#include "decode_thread.h"
#include "audio_internal.h"
#include "ringbuf.h"
#include "playlist.h"

static struct mad_stream g_stream;
static struct mad_frame  g_frame;
static struct mad_synth  g_synth;
static SceUID            g_fd = -1;

bool decoder_open(const char *path) {
    if (g_fd >= 0) { sceIoClose(g_fd); g_fd = -1; }
    g_fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (g_fd < 0) return false;

    mad_stream_init(&g_stream);
    mad_frame_init(&g_frame);
    mad_synth_init(&g_synth);
    return true;
}

void decoder_close(void) {
    mad_synth_finish(&g_synth);
    mad_frame_finish(&g_frame);
    mad_stream_finish(&g_stream);
    if (g_fd >= 0) { sceIoClose(g_fd); g_fd = -1; }
}

// fills MP3 read buffer + feeds mad_stream, returns false on real EOF
static bool decoder_fill(void) {
    static unsigned char in_buf[8192 + MAD_BUFFER_GUARD];
    size_t remaining = g_stream.bufend - g_stream.next_frame;
    memmove(in_buf, g_stream.next_frame, remaining);

    int n = sceIoRead(g_fd, in_buf + remaining, sizeof(in_buf) - remaining);
    if (n <= 0 && remaining == 0) return false; // EOF, nothing left to drain

    mad_stream_buffer(&g_stream, in_buf, remaining + (n > 0 ? n : 0));
    return true;
}

int decode_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    short pcm_chunk[AUDIO_CHUNK_FRAMES * AUDIO_CHANNELS];

    while (g_running) {
        if (g_state != AUDIO_STATE_PLAYING) {
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
                    // real EOF: ask playlist what's next
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
    return 0;
}
