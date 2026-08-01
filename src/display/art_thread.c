#include "display/art_thread.h"
#include "display/bg.h"
#include "display/image.h"
#include "util/logging.h"
#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>

#define TAG "art"

#define ART_THREAD_PRIORITY 0x28
#define ART_THREAD_STACK    0x10000

static SceUID g_job_lock = -1;
static SceUID g_result_lock = -1;
static SceUID g_work_sema = -1;

static ArtJob g_job;
static bool   g_has_job;
static ArtResult g_result;
static bool   g_has_result;
static volatile bool g_running;
static SceUID g_thread_id = -1;

static void free_texture(PSPTexture *t) {
  if (t->pixels) {
    free(t->pixels);
    *t = (PSPTexture){0};
  }
}

static int art_worker(SceSize args, void *argp) {
  (void)args;
  (void)argp;

  while (g_running) {
    sceKernelWaitSema(g_work_sema, 1, NULL);

    if (!g_running)
      break;

    sceKernelWaitSema(g_job_lock, 1, NULL);

    if (!g_has_job) {
      sceKernelSignalSema(g_job_lock, 1);
      continue;
    }

    ArtJob job = g_job;
    g_job = (ArtJob){0};
    g_has_job = false;

    sceKernelSignalSema(g_job_lock, 1);

    PSPTexture art = album_art_from_data(job.art_data, job.art_len);
    PSPTexture bg = build_background_texture(&art);

    free(job.art_data);

    sceKernelWaitSema(g_result_lock, 1, NULL);

    if (g_has_result) {
      free_texture(&g_result.album_art);
      free_texture(&g_result.background);
    }

    g_result.index = job.index;
    g_result.generation = job.generation;
    g_result.album_art = art;
    g_result.background = bg;
    g_has_result = true;

    sceKernelSignalSema(g_result_lock, 1);
  }

  return 0;
}

bool art_submit(int index, uint32_t generation, const metadata_t *meta) {
  ArtJob job = {
      .index = index,
      .generation = generation,
  };

  if (meta->art_data && meta->art_len > 0) {
    job.art_data = malloc(meta->art_len);
    if (!job.art_data)
      return false;
    memcpy(job.art_data, meta->art_data, meta->art_len);
    job.art_len = meta->art_len;
  }

  sceKernelWaitSema(g_job_lock, 1, NULL);
  if (g_has_job)
    free(g_job.art_data);
  g_job = job;
  g_has_job = true;
  sceKernelSignalSema(g_job_lock, 1);

  sceKernelSignalSema(g_work_sema, 1);
  return true;
}

bool art_poll(ArtResult *out) {
  sceKernelWaitSema(g_result_lock, 1, NULL);

  if (!g_has_result) {
    sceKernelSignalSema(g_result_lock, 1);
    return false;
  }

  *out = g_result;
  g_result = (ArtResult){0};
  g_has_result = false;

  sceKernelSignalSema(g_result_lock, 1);
  return true;
}
bool art_thread_init(void) {
  g_job_lock    = sceKernelCreateSema("art_job_lock",    0, 1, 1, NULL);
  g_result_lock = sceKernelCreateSema("art_result_lock", 0, 1, 1, NULL);
  g_work_sema   = sceKernelCreateSema("art_work",        0, 0, 1, NULL);
  if (g_job_lock < 0 || g_result_lock < 0 || g_work_sema < 0) {
    LOG_ERR(TAG, "failed to create art semaphores");
    return false;
  }

  g_running = true;
  g_thread_id = sceKernelCreateThread("art_thread", art_worker,
                                      ART_THREAD_PRIORITY, ART_THREAD_STACK,
                                      THREAD_ATTR_USER | THREAD_ATTR_VFPU, NULL);
  if (g_thread_id < 0) {
    LOG_ERR(TAG, "create art_thread failed: %d", g_thread_id);
    return false;
  }
  if (sceKernelStartThread(g_thread_id, 0, NULL) < 0) {
    LOG_ERR(TAG, "start art_thread failed");
    return false;
  }
  return true;
}

void art_thread_shutdown(void) {
  if (g_thread_id < 0)
    return;

  g_running = false;
  sceKernelSignalSema(g_work_sema, 1);
  sceKernelWaitThreadEnd(g_thread_id, NULL);

  g_thread_id = -1;

  sceKernelWaitSema(g_job_lock, 1, NULL);
  free(g_job.art_data);
  g_job = (ArtJob){0};
  g_has_job = false;
  sceKernelSignalSema(g_job_lock, 1);

  sceKernelWaitSema(g_result_lock, 1, NULL);
  free_texture(&g_result.album_art);
  free_texture(&g_result.background);
  g_result = (ArtResult){0};
  g_has_result = false;
  sceKernelSignalSema(g_result_lock, 1);

  sceKernelDeleteSema(g_job_lock);
  sceKernelDeleteSema(g_result_lock);
  sceKernelDeleteSema(g_work_sema);

  g_job_lock = -1;
  g_result_lock = -1;
  g_work_sema = -1;
}
