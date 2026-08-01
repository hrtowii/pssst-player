#ifndef ART_THREAD_H
#define ART_THREAD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "display/graphics.h"
#include "player/metadata.h"

typedef struct {
  int index;
  uint32_t generation;
  unsigned char *art_data;
  size_t art_len;
} ArtJob;

typedef struct {
  int index;
  uint32_t generation;
  PSPTexture album_art;
  PSPTexture background;
} ArtResult;

bool art_thread_init(void);
void art_thread_shutdown(void);
bool art_submit(int index, uint32_t generation, const metadata_t *meta);
bool art_poll(ArtResult *out);

#endif
