#include <string.h>
#include <stdlib.h>
#include <pspiofilemgr.h>
#include <FLAC/stream_decoder.h>
#include "audio/decoder.h"
#include "util/logging.h"

typedef struct {
    FLAC__StreamDecoder *decoder;
    SceUID fd;
    short pcm_buf[FLAC__MAX_BLOCK_SIZE * 2];
    int pcm_frames;
    int pcm_pos;
    bool eos;
} flac_priv_t;

static FLAC__StreamDecoderReadStatus read_cb(
    const FLAC__StreamDecoder *decoder,
    FLAC__byte buffer[], size_t *bytes,
    void *client_data)
{
    flac_priv_t *p = client_data;
    int n = sceIoRead(p->fd, buffer, *bytes);
    if (n < 0) { *bytes = 0; return FLAC__STREAM_DECODER_READ_STATUS_ABORT; }
    *bytes = n;
    return n == 0
        ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM
        : FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus seek_cb(
    const FLAC__StreamDecoder *decoder,
    FLAC__uint64 absolute_byte_offset,
    void *client_data)
{
    flac_priv_t *p = client_data;
    return sceIoLseek(p->fd, absolute_byte_offset, PSP_SEEK_SET) < 0
        ? FLAC__STREAM_DECODER_SEEK_STATUS_ERROR
        : FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus tell_cb(
    const FLAC__StreamDecoder *decoder,
    FLAC__uint64 *absolute_byte_offset,
    void *client_data)
{
    flac_priv_t *p = client_data;
    long pos = sceIoLseek(p->fd, 0, PSP_SEEK_CUR);
    if (pos < 0) return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    *absolute_byte_offset = pos;
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus length_cb(
    const FLAC__StreamDecoder *decoder,
    FLAC__uint64 *stream_length,
    void *client_data)
{
    flac_priv_t *p = client_data;
    long cur = sceIoLseek(p->fd, 0, PSP_SEEK_CUR);
    long end = sceIoLseek(p->fd, 0, PSP_SEEK_END);
    sceIoLseek(p->fd, cur, PSP_SEEK_SET);
    if (end < 0) return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    *stream_length = end;
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool eof_cb(
    const FLAC__StreamDecoder *decoder,
    void *client_data)
{
    (void)decoder;
    flac_priv_t *p = client_data;
    return p->eos;
}

static FLAC__StreamDecoderWriteStatus write_cb(
    const FLAC__StreamDecoder *decoder,
    const FLAC__Frame *frame,
    const FLAC__int32 * const buffer[],
    void *client_data)
{
    flac_priv_t *p = client_data;
    int n = frame->header.blocksize;
    int ch = frame->header.channels;
    if (ch != 2) {
        for (int i = 0; i < n; i++) {
            p->pcm_buf[i * 2]     = (short)buffer[0][i];
            p->pcm_buf[i * 2 + 1] = (short)buffer[0][i];
        }
    } else {
        for (int i = 0; i < n; i++) {
            p->pcm_buf[i * 2]     = (short)buffer[0][i];
            p->pcm_buf[i * 2 + 1] = (short)buffer[1][i];
        }
    }
    p->pcm_frames = n;
    p->pcm_pos = 0;
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_cb(
    const FLAC__StreamDecoder *decoder,
    const FLAC__StreamMetadata *metadata,
    void *client_data)
{
    (void)decoder; (void)metadata; (void)client_data;
}

static void error_cb(
    const FLAC__StreamDecoder *decoder,
    FLAC__StreamDecoderErrorStatus status,
    void *client_data)
{
    (void)decoder; (void)status; (void)client_data;
}

static bool flac_open(decoder_t *self, const char *path)
{
    flac_priv_t *p = calloc(1, sizeof(flac_priv_t));
    if (!p) return false;

    p->fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (p->fd < 0) { free(p); return false; }

    p->decoder = FLAC__stream_decoder_new();
    if (!p->decoder) { sceIoClose(p->fd); free(p); return false; }

    FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_stream(
        p->decoder,
        read_cb, seek_cb, tell_cb, length_cb, eof_cb,
        write_cb, metadata_cb, error_cb,
        p
    );

    if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        LOG_ERR("FLAC", "init_stream failed: %d", (int)status);
        FLAC__stream_decoder_delete(p->decoder);
        sceIoClose(p->fd);
        free(p);
        return false;
    }

    if (!FLAC__stream_decoder_process_until_end_of_metadata(p->decoder)) {
        FLAC__stream_decoder_finish(p->decoder);
        FLAC__stream_decoder_delete(p->decoder);
        sceIoClose(p->fd);
        free(p);
        return false;
    }

    self->priv = p;
    return true;
}

static int flac_read_pcm(decoder_t *self, short *buf, int frames)
{
    flac_priv_t *p = self->priv;
    if (!p || p->eos) return 0;

    int written = 0;

    while (written < frames) {
        if (p->pcm_pos < p->pcm_frames) {
            int avail = p->pcm_frames - p->pcm_pos;
            int n = frames - written;
            if (n > avail) n = avail;
            memcpy(buf + written * 2, p->pcm_buf + p->pcm_pos * 2, n * 2 * sizeof(short));
            p->pcm_pos += n;
            written += n;
        } else {
            if (!FLAC__stream_decoder_process_single(p->decoder)) {
                FLAC__StreamDecoderState st = FLAC__stream_decoder_get_state(p->decoder);
                if (st == FLAC__STREAM_DECODER_END_OF_STREAM)
                    p->eos = true;
                break;
            }
            if (FLAC__stream_decoder_get_state(p->decoder) == FLAC__STREAM_DECODER_END_OF_STREAM) {
                p->eos = true;
                break;
            }
        }
    }

    return written;
}

static void flac_close(decoder_t *self)
{
    flac_priv_t *p = self->priv;
    if (!p) return;
    FLAC__stream_decoder_finish(p->decoder);
    FLAC__stream_decoder_delete(p->decoder);
    if (p->fd >= 0) sceIoClose(p->fd);
    free(p);
    self->priv = NULL;
}

decoder_t decoder_flac = {
    .open    = flac_open,
    .read_pcm = flac_read_pcm,
    .close   = flac_close,
    .priv    = NULL,
};
