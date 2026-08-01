#include <string.h>
#include <stdlib.h>
#include <pspiofilemgr.h>
#include <mad.h>
#include "audio/audio.h"
#include "audio/decoder.h"
#include "util/logging.h"
#define MP3_MAX_FRAME_SAMPLES 1152
typedef struct {
    struct mad_stream stream;
    struct mad_frame  frame;
    struct mad_synth  synth;
    SceUID fd;
    unsigned char in_buf[32768 + MAD_BUFFER_GUARD];
    short leftover[MP3_MAX_FRAME_SAMPLES * AUDIO_CHANNELS];
    int leftover_frames;
} mp3_priv_t;

static inline signed int scale(mad_fixed_t sample)
{
    sample += (1L << (MAD_F_FRACBITS - 16));

    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static bool mp3_fill(mp3_priv_t *p)
{
    size_t remaining = 0;

    if (p->stream.next_frame) {
        remaining = p->stream.bufend - p->stream.next_frame;
        memcpy(p->in_buf, p->stream.next_frame, remaining);
    }

    int n = sceIoRead(p->fd,
                      p->in_buf + remaining,
                      32768 - remaining);
// LOG_DEBUG("MP3",
//     "remaining=%u read=%d",
//     (unsigned)remaining,
    // n);
    // for SOME FUCKING REASON when the file ends the remaining gets stuck at 480 because idk ??
    if (n <= 0)
        return false;

    size_t valid = remaining + (n > 0 ? n : 0);

    memset(p->in_buf + valid, 0, MAD_BUFFER_GUARD);
    mad_stream_buffer(&p->stream, p->in_buf, valid);

    return true;
}

static bool mp3_open(decoder_t *self, const char *path) {
    mp3_priv_t *p = calloc(1, sizeof(mp3_priv_t));
    if (!p) return false;

    p->fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (p->fd < 0) { free(p); return false; }

    mad_stream_init(&p->stream);
    mad_frame_init(&p->frame);
    mad_synth_init(&p->synth);

    if (!mp3_fill(p)) {
        mad_synth_finish(&p->synth);
        mad_frame_finish(&p->frame);
        mad_stream_finish(&p->stream);
        sceIoClose(p->fd);
        free(p);
        return false;
    }

    self->priv = p;
    return true;
}

static int mp3_read_pcm(decoder_t *self, short *buf, int frames)
{
    mp3_priv_t *p = self->priv;
    if (!p || p->fd < 0)
        return 0;

    int written = 0;

    if (p->leftover_frames > 0) {
        int take = p->leftover_frames;
        if (take > frames)
            take = frames;
        memcpy(buf, p->leftover, take * AUDIO_CHANNELS * sizeof(short));
        written += take;
        p->leftover_frames -= take;
        if (p->leftover_frames > 0)
            memmove(p->leftover,
                    p->leftover + take * AUDIO_CHANNELS,
                    p->leftover_frames * AUDIO_CHANNELS * sizeof(short));
        if (written >= frames)
            return written;
    }

    for (;;) {
        int ret = mad_frame_decode(&p->frame, &p->stream);

        if (ret == 0) {
            mad_synth_frame(&p->synth, &p->frame);

            int n = p->synth.pcm.length;
            const mad_fixed_t *left  = p->synth.pcm.samples[0];
            const mad_fixed_t *right =
                (p->synth.pcm.channels == 2)
                    ? p->synth.pcm.samples[1]
                    : p->synth.pcm.samples[0];

            int room = frames - written;
            if (n > room) {
                for (int i = 0; i < room; i++) {
                    buf[(written + i) * 2]     = scale(left[i]);
                    buf[(written + i) * 2 + 1] = scale(right[i]);
                }
                for (int i = room; i < n; i++) {
                    p->leftover[(i - room) * 2]     = scale(left[i]);
                    p->leftover[(i - room) * 2 + 1] = scale(right[i]);
                }
                p->leftover_frames = n - room;
                return frames;
            }

            for (int i = 0; i < n; i++) {
                buf[(written + i) * 2]     = scale(left[i]);
                buf[(written + i) * 2 + 1] = scale(right[i]);
            }
            written += n;

            if (written >= frames)
                return written;

            continue;
        }

        if (p->stream.error == MAD_ERROR_BUFLEN) {

            if (!mp3_fill(p))
                return written;

            continue;
        }

        if (MAD_RECOVERABLE(p->stream.error))
            continue;

        return written;
    }
}

static void mp3_close(decoder_t *self) {
    mp3_priv_t *p = self->priv;
    if (!p) return;
    mad_synth_finish(&p->synth);
    mad_frame_finish(&p->frame);
    mad_stream_finish(&p->stream);
    if (p->fd >= 0) sceIoClose(p->fd);
    free(p);
    self->priv = NULL;
}
// externed in audio/decoder.h
decoder_t decoder_mp3 = {
    .open    = mp3_open,
    .read_pcm = mp3_read_pcm,
    .close   = mp3_close,
    .priv    = NULL,
};
