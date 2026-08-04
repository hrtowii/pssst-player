#include "audio/audio.h"
#include "audio/decoder.h"
#include "util/logging.h"
#include <opus/opusfile.h>
#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>
#include <stdlib.h>
#include <string.h>

#define TAG "OPUS"
struct opus_priv_t {
  OggOpusFile *handle;
  int channels;
  // note: sample rate is 48k. whenever playing opus files just force 48k
};

static int psp_opus_read(void *_stream, unsigned char *_ptr, int _buf_size) {
  SceUID fd = *(SceUID *)_stream;
  return sceIoRead(fd, _ptr, _buf_size);
}

static int psp_opus_seek(void *_stream, opus_int64 _offset, int _whence) {
  SceUID fd = *(SceUID *)_stream;
  SceOff res = sceIoLseek(fd, (SceOff)_offset,
                          _whence); // surely casting a 64 bit to a 32 bit will
                                    // have No Consequences Whatsoever
  return (res < 0) ? -1 : 0;
}

static opus_int64 psp_opus_tell(void *_stream) {
  SceUID fd = *(SceUID *)_stream;
  return (opus_int64)sceIoLseek(fd, 0, PSP_SEEK_CUR);
}

static int psp_opus_close(void *_stream) {
  SceUID fd = *(SceUID *)_stream;
  sceIoClose(fd);
  free(_stream);
  return 0;
}

static const OpusFileCallbacks psp_callbacks = {.read = psp_opus_read,
                                                .seek = psp_opus_seek,
                                                .tell = psp_opus_tell,
                                                .close = psp_opus_close};

static bool opus_open(decoder_t *self, const char *path) {
  SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
  if (fd < 0) {
    LOG_ERR(TAG, "failed to open opus file %s", path);
    return false;
  }
  SceUID *fd_ptr = malloc(sizeof(SceUID));
  if (!fd_ptr) {
    sceIoClose(fd);
    return false;
  }
  *fd_ptr = fd;
  int error = 0;
  OggOpusFile *of = op_open_callbacks(fd_ptr, &psp_callbacks, NULL, 0, &error);
  if (!of) {
    LOG_ERR(TAG, "libopusfile failed to open stream. Error code: %d", error);
    sceIoClose(fd);
    free(fd_ptr);
    return false;
  }

  struct opus_priv_t *priv = malloc(sizeof(struct opus_priv_t));
  if (!priv) {
    op_free(of);
    return false;
  }

  priv->handle = of;
  priv->channels = op_channel_count(of, -1);

  LOG_DEBUG(TAG, "opened %s (%d channels, 48kHz forced)", path, priv->channels);

  self->priv = priv;
  return true;
}

static void opus_close(decoder_t *self) {
  struct opus_priv_t *priv = (struct opus_priv_t *)self->priv;

  if (priv) {
    if (priv->handle) {
      op_free(priv->handle);
    }
    free(priv);
  }
}

static int opus_read_pcm(decoder_t *self, int16_t *buffer, int max_frames) {
  struct opus_priv_t *priv = (struct opus_priv_t *)self->priv;
  int max_samples = max_frames * priv->channels;
  int frames_decoded = op_read(priv->handle, buffer, max_samples, NULL);
  if (frames_decoded < 0) {
    LOG_ERR(TAG, "Decode error: %d", frames_decoded);
    return -1;
  }
  // if 0, eof
  return frames_decoded;
}

decoder_t decoder_opus = {
    .open = opus_open,
    .read_pcm = opus_read_pcm,
    .close = opus_close,
    .priv = NULL,
};
