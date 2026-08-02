#include "audio/decode_thread.h"
#include "audio/audio_internal.h"
#include "audio/decoder.h"
#include "player/playlist.h"
#include "player/metadata.h"
#include "util/logging.h"
#include "util/ringbuf.h"
#include <pspiofilemgr.h>
#include <psprtc.h>
#include <string.h>

#define TAG "decode_thread"

static decoder_t g_decoder;
static bool g_decoder_open = false;

static void decoder_close(void) {
  if (!g_decoder_open)
    return;
  g_decoder.close(&g_decoder);
  g_decoder_open = false;
  g_output_frames = 0;
}

static const decoder_t *decoder_from_path(const char *path) {
  const char *ext = strrchr(path, '.');

  if (!ext || ext == path || ext[1] == '\0')
    return NULL;

  ext++;

  if (strcasecmp(ext, "mp3") == 0)
    return &decoder_mp3;

  if (strcasecmp(ext, "flac") == 0)
    return &decoder_flac;

  LOG_ERR(TAG, "unsupported extension: .%s", ext);
  return NULL;
}

static bool decoder_open(const char *path) {
  if (g_decoder_open)
    decoder_close();

  const decoder_t *decoder = decoder_from_path(path);
  if (!decoder)
    return false;

  memcpy(&g_decoder, decoder, sizeof(g_decoder));

  if (!g_decoder.open(&g_decoder, path)) {
    LOG_ERR(TAG, "decoder_open failed: %s", path);
    return false;
  }

  g_decoder_open = true;
// this is kinda scuffed bc now metadata is used by both ui and decode...
  metadata_t meta;
  metadata_loader_t *loader = metadata_loader_for(path);
  int rate = 0;
  if (loader && loader->load(loader, path, &meta)) {
    rate = meta.sample_rate;
    metadata_free(&meta);
  }
  g_pending_rate = rate;
  ring_reset(&g_ring);

  return true;
}

static void handle_pending_switch(void) {
  if (g_switch_kind == SWITCH_NONE)
    return;

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

  if (kind == SWITCH_NONE)
    return;
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
  (void)args;
  (void)argp;
  short pcm_chunk[AUDIO_CHUNK_FRAMES * AUDIO_CHANNELS]
      __attribute__((aligned(64)));

  while (g_running) {

    handle_pending_switch();

    if (g_state != AUDIO_STATE_PLAYING || !g_decoder_open) {
      sceKernelDelayThread(10 * 100);
      continue;
    }
    if (ring_frames_available(&g_ring) >= 32768) {
      sceKernelDelayThread(5000);
      continue;
    }
    int n = g_decoder.read_pcm(&g_decoder, pcm_chunk, AUDIO_CHUNK_FRAMES);
    if (n <= 0) {
      const char *next = playlist_advance(&g_pl);
      decoder_close();
      if (!next || !decoder_open(next)) {
        g_state = AUDIO_STATE_STOPPED;
      }
      continue;
    }

    ring_push(&g_ring, pcm_chunk, n);
    sceKernelDelayThread(1000);
  }

  decoder_close();
  return 0;
}
