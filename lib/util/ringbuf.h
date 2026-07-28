#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "audio/audio_internal.h"

int ring_frames_free(ring_buffer_t *rb);
int ring_frames_available(ring_buffer_t *rb);
int ring_push(ring_buffer_t *rb, const short *frames, int count);
int ring_pop(ring_buffer_t *rb, short *out, int count);

#endif
