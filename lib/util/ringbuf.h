#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <pspkernel.h>
#include "audio/audio.h"

typedef struct {
    short  data[AUDIO_RINGBUF_FRAMES * AUDIO_CHANNELS];
    volatile int head;
    volatile int tail;
    SceUID lock;
} ring_buffer_t;

int ring_frames_free(ring_buffer_t *rb);
int ring_frames_available(ring_buffer_t *rb);
int ring_push(ring_buffer_t *rb, const short *frames, int count);
int ring_pop(ring_buffer_t *rb, short *out, int count);
void ring_reset(ring_buffer_t *rb);

#endif
