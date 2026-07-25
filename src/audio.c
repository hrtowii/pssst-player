#include "audio.h"
#include "audio_internal.h"
#include "decode_thread.h"
#include "mt19937.h"
#include "output_thread.h"
#include "playlist.h"
#include "ringbuf.h"
#include <pspaudio.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <stdlib.h>
#include <string.h>

// ---- global state definitions (declared extern in audio_internal.h) ----
playlist_t g_pl;
ring_buffer_t g_ring;
volatile audio_state_t g_state = AUDIO_STATE_STOPPED;
volatile bool g_running = false;
int g_audio_channel = -1;

static SceUID g_decode_thread_id = -1;
static SceUID g_output_thread_id = -1;

bool audio_init(void) {
  u64 tick;
  sceRtcGetCurrentTick(&tick);
  mt_seed((uint32_t)tick);
  memset(&g_pl, 0, sizeof(g_pl));
  memset(&g_ring, 0, sizeof(g_ring));
  g_pl.current = -1;
  g_pl.lock = sceKernelCreateSema("pl_lock", 0, 1, 1, NULL);
  g_ring.lock = sceKernelCreateSema("ring_lock", 0, 1, 1, NULL);

  g_audio_channel = sceAudioChReserve(
      PSP_AUDIO_NEXT_CHANNEL, AUDIO_CHUNK_FRAMES, PSP_AUDIO_FORMAT_STEREO);
  if (g_audio_channel < 0)
    return false;

  g_running = true;

  g_decode_thread_id = sceKernelCreateThread("decode_thread", decode_thread,
                                             0x18, 0x10000, 0, NULL);
  sceKernelStartThread(g_decode_thread_id, 0, NULL);

  g_output_thread_id = sceKernelCreateThread("output_thread", output_thread,
                                             0x12, 0x10000, 0, NULL);
  sceKernelStartThread(g_output_thread_id, 0, NULL);

  return true;
}

void audio_shutdown(void) {
  g_running = false;
  decoder_close();
  sceAudioChRelease(g_audio_channel);
}

bool audio_playlist_set(const char **paths, int count) {
  if (count > AUDIO_MAX_TRACKS)
    return false;
  sceKernelWaitSema(g_pl.lock, 1, NULL);
  for (int i = 0; i < g_pl.count; i++)
    free(g_pl.paths[i]);
  for (int i = 0; i < count; i++)
    g_pl.paths[i] = strdup(paths[i]);
  g_pl.count = count;
  g_pl.current = -1;
  sceKernelSignalSema(g_pl.lock, 1);
  return true;
}

void audio_play_index(int index) {
  sceKernelWaitSema(g_pl.lock, 1, NULL);
  if (index < 0 || index >= g_pl.count) {
    sceKernelSignalSema(g_pl.lock, 1);
    return;
  }
  g_pl.current = index;
  const char *path = g_pl.paths[index];
  sceKernelSignalSema(g_pl.lock, 1);

  decoder_close();
  if (decoder_open(path))
    g_state = AUDIO_STATE_PLAYING;
}

void audio_play(void) {
  if (g_state == AUDIO_STATE_PAUSED)
    g_state = AUDIO_STATE_PLAYING;
}
void audio_pause(void) {
  if (g_state == AUDIO_STATE_PLAYING)
    g_state = AUDIO_STATE_PAUSED;
}
void audio_stop(void) {
  g_state = AUDIO_STATE_STOPPED;
  decoder_close();
}

void audio_next(void) {
  const char *path = playlist_advance(&g_pl);
  decoder_close();
  if (path && decoder_open(path))
    g_state = AUDIO_STATE_PLAYING;
  else
    g_state = AUDIO_STATE_STOPPED;
}

void audio_prev(void) {
  sceKernelWaitSema(g_pl.lock, 1, NULL);
  if (g_pl.count == 0) {
    sceKernelSignalSema(g_pl.lock, 1);
    return;
  }
  int prev = g_pl.current - 1;
  if (prev < 0) {
    if (g_pl.repeat == REPEAT_ALL)
      prev = g_pl.count - 1;
    else
      prev = 0; // clamp at start rather than wrap
  }
  g_pl.current = prev;
  const char *path = g_pl.paths[prev];
  sceKernelSignalSema(g_pl.lock, 1);

  decoder_close();
  if (decoder_open(path))
    g_state = AUDIO_STATE_PLAYING;
  else
    g_state = AUDIO_STATE_STOPPED;
}

void audio_set_repeat(repeat_mode_t mode) { g_pl.repeat = mode; }
void audio_set_shuffle(bool on) { g_pl.shuffle = on; }

audio_state_t audio_get_state(void) { return g_state; }
int audio_get_current_index(void) { return g_pl.current; }
const char *audio_get_current_path(void) {
  return (g_pl.current >= 0 && g_pl.current < g_pl.count)
             ? g_pl.paths[g_pl.current]
             : NULL;
}
