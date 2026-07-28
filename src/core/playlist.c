#include "core/playlist.h"
#include "util/mt19937.h"
#include <psprtc.h>

const char *playlist_advance(playlist_t *pl) {
  sceKernelWaitSema(pl->lock, 1, NULL);
  if (pl->count == 0) {
    sceKernelSignalSema(pl->lock, 1);
    return NULL;
  }

  if (pl->repeat == REPEAT_ONE) {
    sceKernelSignalSema(pl->lock, 1);
    return pl->paths[pl->current];
  }

  int next =
      pl->shuffle
          ? (pl->current + 1 + mt_range(pl->count > 1 ? pl->count - 1 : 1)) %
                pl->count
          : pl->current + 1;

  if (next >= pl->count) {
    if (pl->repeat == REPEAT_ALL)
      next = 0;
    else {
      sceKernelSignalSema(pl->lock, 1);
      return NULL;
    }
  }
  pl->current = next;
  const char *path = pl->paths[pl->current];
  sceKernelSignalSema(pl->lock, 1);
  return path;
}
