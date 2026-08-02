#include "audio/audio.h"
#include "audio/audio_internal.h"
#include "audio/decode_thread.h"
#include "audio/output_thread.h"
#include "player/playlist.h"
#include "util/logging.h"
#include "util/mt19937.h"
#include "util/ringbuf.h"
#include <pspaudio.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <stdlib.h>
#include <string.h>

#define TAG "audio"

playlist_t g_pl;
ring_buffer_t g_ring;
volatile audio_state_t g_state = AUDIO_STATE_STOPPED;
volatile bool g_running = false;
int g_audio_channel = -1;

volatile switch_kind_t g_switch_kind = SWITCH_NONE;
char g_switch_path[MAX_PATH];
SceUID g_switch_lock;

volatile int g_output_frames = 0;

volatile int g_sample_rate = 44100;
volatile int g_pending_rate = 0;

static bool g_src_reserved = false;

int audio_reconfigure_output(int sample_rate) {
  bool want_src = (sample_rate == 48000);

  if (want_src == g_src_reserved)
    return 0;

  if (g_src_reserved) {
    sceAudioSRCChRelease();
    g_src_reserved = false;
    LOG_INFO(TAG, "output -> hardware 44.1kHz");
  }
  if (g_audio_channel >= 0) {
    sceAudioChRelease(g_audio_channel);
    g_audio_channel = -1;
  }

  if (want_src) {
    if (sceAudioSRCChReserve(AUDIO_CHUNK_FRAMES, 48000, 2) < 0) {
      LOG_ERR(TAG, "sceAudioSRCChReserve failed");
      return -1;
    }
    g_src_reserved = true;
    LOG_INFO(TAG, "output -> SRC 48kHz");
    return 0;
  }

  int ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_CHUNK_FRAMES,
                             PSP_AUDIO_FORMAT_STEREO);
  if (ch < 0) {
    LOG_ERR(TAG, "sceAudioChReserve failed");
    return -1;
  }
  g_audio_channel = ch;
  return 0;
}

static SceUID g_decode_thread_id = -1;
static SceUID g_output_thread_id = -1;

static void request_switch_to(const char *path) {
  sceKernelWaitSema(g_switch_lock, 1, NULL);
  strncpy(g_switch_path, path, sizeof(g_switch_path) - 1);
  g_switch_path[sizeof(g_switch_path) - 1] = '\0';
  g_switch_kind = SWITCH_TO_PATH;
  sceKernelSignalSema(g_switch_lock, 1);
}

static void request_stop(void) {
  sceKernelWaitSema(g_switch_lock, 1, NULL);
  g_switch_kind = SWITCH_STOP;
  sceKernelSignalSema(g_switch_lock, 1);
}

bool audio_init(void) {
  memset(&g_pl, 0, sizeof(g_pl));
  memset(&g_ring, 0, sizeof(g_ring));
  g_pl.current = -1;

  g_pl.lock = sceKernelCreateSema("pl_lock", 0, 1, 1, NULL);
  g_ring.lock = sceKernelCreateSema("ring_lock", 0, 1, 1, NULL);
  g_switch_lock = sceKernelCreateSema("switch_lock", 0, 1, 1, NULL);

  u64 tick;
  sceRtcGetCurrentTick(&tick);
  mt_seed((uint32_t)tick);

  g_audio_channel = sceAudioChReserve(
      PSP_AUDIO_NEXT_CHANNEL, AUDIO_CHUNK_FRAMES, PSP_AUDIO_FORMAT_STEREO);
  if (g_audio_channel < 0) {
    LOG_ERR(TAG, "sceAudioChReserve failed: %d", g_audio_channel);
    return false;
  }

  g_running = true;

  g_decode_thread_id = sceKernelCreateThread("decode_thread", decode_thread,
                                             0x20, 0x10000, 0, NULL);
  if (g_decode_thread_id < 0) {
    LOG_ERR(TAG, "create decode_thread failed: %d", g_decode_thread_id);
    return false;
  }
  if (sceKernelStartThread(g_decode_thread_id, 0, NULL) < 0) {
    LOG_ERR(TAG, "start decode_thread failed");
    return false;
  }

  g_output_thread_id = sceKernelCreateThread("output_thread", output_thread,
                                             0x18, 0x10000, 0, NULL);
  if (g_output_thread_id < 0) {
    LOG_ERR(TAG, "create output_thread failed: %d", g_output_thread_id);
    return false;
  }
  if (sceKernelStartThread(g_output_thread_id, 0, NULL) < 0) {
    LOG_ERR(TAG, "start output_thread failed");
    return false;
  }

  return true;
}

void audio_shutdown(void) {
  request_stop();
  g_running = false; // decode_thread + output_thread both check this and exit
  sceKernelWaitThreadEnd(g_decode_thread_id, NULL);
  sceKernelWaitThreadEnd(g_output_thread_id, NULL);
  if (g_src_reserved) {
    sceAudioSRCChRelease();
    g_src_reserved = false;
  }
  if (g_audio_channel >= 0)
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

bool audio_playlist_add(const char *path) {
  sceKernelWaitSema(g_pl.lock, 1, NULL);
  if (g_pl.count >= AUDIO_MAX_TRACKS) {
    sceKernelSignalSema(g_pl.lock, 1);
    return false;
  }
  g_pl.paths[g_pl.count] = strdup(path);
  g_pl.count++;
  sceKernelSignalSema(g_pl.lock, 1);
  return true;
}

void audio_playlist_clear(void) {
  sceKernelWaitSema(g_pl.lock, 1, NULL);
  for (int i = 0; i < g_pl.count; i++)
    free(g_pl.paths[i]);
  g_pl.count = 0;
  g_pl.current = -1;
  sceKernelSignalSema(g_pl.lock, 1);
}

void audio_play_index(int index) {
  sceKernelWaitSema(g_pl.lock, 1, NULL);
  if (index < 0 || index >= g_pl.count) {
    sceKernelSignalSema(g_pl.lock, 1);
    return;
  }
  g_pl.current = index;
  const char *path = g_pl.paths[index];
  request_switch_to(path);
  sceKernelSignalSema(g_pl.lock, 1);
}

void audio_play(void) {
  if (g_state == AUDIO_STATE_PAUSED)
    g_state = AUDIO_STATE_PLAYING;
}

void audio_pause(void) {
  if (g_state == AUDIO_STATE_PLAYING)
    g_state = AUDIO_STATE_PAUSED;
}

void audio_stop(void) { request_stop(); }

void audio_next(void) {
  const char *path = playlist_advance(&g_pl);
  if (path)
    request_switch_to(path);
  else
    request_stop();
}

void audio_prev(void) {
  sceKernelWaitSema(g_pl.lock, 1, NULL);
  if (g_pl.count == 0) {
    sceKernelSignalSema(g_pl.lock, 1);
    return;
  }
  int prev = g_pl.current - 1;
  if (prev < 0)
    prev = (g_pl.repeat == REPEAT_ALL) ? g_pl.count - 1 : 0;
  g_pl.current = prev;
  const char *path = g_pl.paths[prev];
  sceKernelSignalSema(g_pl.lock, 1);

  request_switch_to(path);
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
int audio_get_current_second(void) {
  return g_output_frames / (g_sample_rate > 0 ? g_sample_rate : 44100);
}
