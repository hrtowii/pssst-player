#include "audio/audio.h"
#include "audio/decoder.h"
#include "audio/resampler.h"
#include "util/logging.h"
#include <FLAC/stream_decoder.h>
#include <pspiofilemgr.h>
#include <stdlib.h>
#include <string.h>

#define TAG "FLAC"

#define FLAC_RESAMPLE_BUFFER_FRAMES (FLAC__MAX_BLOCK_SIZE * 2)
#define FLAC_PCM_BUFFER_FRAMES (FLAC__MAX_BLOCK_SIZE + 1)

typedef struct {
  FLAC__StreamDecoder *decoder;
  SceUID fd;

  short pcm_buf[FLAC_PCM_BUFFER_FRAMES * 2];
  int pcm_frames;
  int pcm_pos;

  short resample_buf[FLAC_RESAMPLE_BUFFER_FRAMES * 2];

  resampler_t resampler;
  int sample_rate;
  bool eos;
} flac_priv_t;

static FLAC__StreamDecoderReadStatus read_cb(const FLAC__StreamDecoder *decoder,
                                             FLAC__byte buffer[], size_t *bytes,
                                             void *client_data) {
  flac_priv_t *p = client_data;
  int n = sceIoRead(p->fd, buffer, *bytes);
  if (n < 0) {
    *bytes = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }
  *bytes = n;
  return n == 0 ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM
                : FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus seek_cb(const FLAC__StreamDecoder *decoder,
                                             FLAC__uint64 absolute_byte_offset,
                                             void *client_data) {
  flac_priv_t *p = client_data;
  return sceIoLseek(p->fd, absolute_byte_offset, PSP_SEEK_SET) < 0
             ? FLAC__STREAM_DECODER_SEEK_STATUS_ERROR
             : FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus tell_cb(const FLAC__StreamDecoder *decoder,
                                             FLAC__uint64 *absolute_byte_offset,
                                             void *client_data) {
  flac_priv_t *p = client_data;
  long pos = sceIoLseek(p->fd, 0, PSP_SEEK_CUR);
  if (pos < 0)
    return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
  *absolute_byte_offset = pos;
  return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus
length_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length,
          void *client_data) {
  flac_priv_t *p = client_data;
  long cur = sceIoLseek(p->fd, 0, PSP_SEEK_CUR);
  long end = sceIoLseek(p->fd, 0, PSP_SEEK_END);
  sceIoLseek(p->fd, cur, PSP_SEEK_SET);
  if (end < 0)
    return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
  *stream_length = end;
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool eof_cb(const FLAC__StreamDecoder *decoder,
                         void *client_data) {
  (void)decoder;
  flac_priv_t *p = client_data;
  return p->eos;
}

static short flac_sample_to_s16(FLAC__int32 sample, unsigned bits_per_sample) {
  if (bits_per_sample > 16)
    sample >>= bits_per_sample - 16;
  else if (bits_per_sample < 16)
    sample <<= 16 - bits_per_sample;

  return (short)sample;
}

static FLAC__StreamDecoderWriteStatus
write_cb(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,
         const FLAC__int32 *const buffer[], void *client_data) {
  (void)decoder;

  flac_priv_t *p = client_data;
  int n = frame->header.blocksize;
  int ch = frame->header.channels;
  unsigned bps = frame->header.bits_per_sample;
  int remaining = p->pcm_frames - p->pcm_pos;

  if (remaining > 0 && p->pcm_pos > 0) {

    memmove(p->pcm_buf,

            p->pcm_buf + p->pcm_pos * 2,

            remaining * 2 * sizeof(short));
  }
  p->pcm_frames = remaining;

  p->pcm_pos = 0;
  if (p->pcm_frames + n > FLAC_PCM_BUFFER_FRAMES) {

    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  int base = p->pcm_frames;

  if (ch == 1) {
    // fallback to mono bc ngl i dont think the psp can play multiple channels
    // either

    for (int i = 0; i < n; i++) {
      // downsample from 24 to 16 bits because the PSP only supports 16/44.1khz,
      // so 48khz songs are also fcked basically
      short sample = flac_sample_to_s16(buffer[0][i], bps);

      p->pcm_buf[(base + i) * 2] = sample;
      p->pcm_buf[(base + i) * 2 + 1] = sample;
    }
  } else if (ch == 2) {
    for (int i = 0; i < n; i++) {
      p->pcm_buf[(base + i) * 2] = flac_sample_to_s16(buffer[0][i], bps);

      p->pcm_buf[(base + i) * 2 + 1] = flac_sample_to_s16(buffer[1][i], bps);
    }
  } else {
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  p->pcm_frames += n;

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_cb(const FLAC__StreamDecoder *decoder,
                        const FLAC__StreamMetadata *metadata,
                        void *client_data) {
  (void)decoder;
  flac_priv_t *p = client_data;

  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {

    p->sample_rate = metadata->data.stream_info.sample_rate;
  }
}

static void error_cb(const FLAC__StreamDecoder *decoder,
                     FLAC__StreamDecoderErrorStatus status, void *client_data) {
  (void)decoder;
  (void)status;
  (void)client_data;
}

static bool flac_open(decoder_t *self, const char *path) {
  flac_priv_t *p = calloc(1, sizeof(flac_priv_t));
  if (!p)
    return false;

  p->fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
  if (p->fd < 0) {
    free(p);
    return false;
  }

  p->decoder = FLAC__stream_decoder_new();
  if (!p->decoder) {
    sceIoClose(p->fd);
    free(p);
    return false;
  }

  FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_stream(
      p->decoder, read_cb, seek_cb, tell_cb, length_cb, eof_cb, write_cb,
      metadata_cb, error_cb, p);

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

  if (resampler_init(&p->resampler, p->sample_rate, AUDIO_SAMPLE_RATE) < 0) {
    LOG_ERR(TAG, "resample init FAIL");
    FLAC__stream_decoder_finish(p->decoder);

    FLAC__stream_decoder_delete(p->decoder);

    sceIoClose(p->fd);
    free(p);

    return false;
  }

  self->priv = p;
  return true;
}

static int flac_read_pcm(decoder_t *self, short *buf, int frames) {
  flac_priv_t *p = self->priv;

  if (!p || !buf || frames <= 0)
    return 0;

  int written = 0;

  while (written < frames) {
    if (p->pcm_frames - p->pcm_pos < 2) {
      if (p->eos)
        break;

      if (!FLAC__stream_decoder_process_single(p->decoder)) {
        FLAC__StreamDecoderState state =
            FLAC__stream_decoder_get_state(p->decoder);

        if (state == FLAC__STREAM_DECODER_END_OF_STREAM)
          p->eos = true;

        break;
      }

      if (FLAC__stream_decoder_get_state(p->decoder) ==
          FLAC__STREAM_DECODER_END_OF_STREAM) {
        p->eos = true;
      }

      /*
       * process_single() may process metadata or otherwise produce
       * no PCM. Go around the loop and try again.
       */
      if (p->pcm_frames - p->pcm_pos < 2)
        continue;
    }

    int available = p->pcm_frames - p->pcm_pos;
    int output_space = frames - written;

    int output_cap = output_space;
    if (output_cap > FLAC_RESAMPLE_BUFFER_FRAMES)
      output_cap = FLAC_RESAMPLE_BUFFER_FRAMES;

    int input_consumed = 0;

    int produced =
        resampler_process(&p->resampler, p->pcm_buf + p->pcm_pos * 2, available,
                          p->resample_buf, output_cap, &input_consumed);

    if (produced > 0) {
      memcpy(buf + written * 2, p->resample_buf, produced * 2 * sizeof(short));

      written += produced;
    }

    p->pcm_pos += input_consumed;

    /*
     * The resampler needs more input. Do not return zero to the
     * decode thread; decode another FLAC block and let write_cb()
     * preserve and append the remaining input.
     */
    if (produced == 0 && input_consumed == 0) {
      if (p->eos)
        break;

      if (!FLAC__stream_decoder_process_single(p->decoder)) {
        FLAC__StreamDecoderState state =
            FLAC__stream_decoder_get_state(p->decoder);

        if (state == FLAC__STREAM_DECODER_END_OF_STREAM)
          p->eos = true;

        break;
      }

      if (FLAC__stream_decoder_get_state(p->decoder) ==
          FLAC__STREAM_DECODER_END_OF_STREAM) {
        p->eos = true;
      }
    }
  }

  return written;
}

static void flac_close(decoder_t *self) {
  flac_priv_t *p = self->priv;
  if (!p)
    return;
  FLAC__stream_decoder_finish(p->decoder);
  FLAC__stream_decoder_delete(p->decoder);
  if (p->fd >= 0)
    sceIoClose(p->fd);
  free(p);
  self->priv = NULL;
}

decoder_t decoder_flac = {
    .open = flac_open,
    .read_pcm = flac_read_pcm,
    .close = flac_close,
    .priv = NULL,
};
