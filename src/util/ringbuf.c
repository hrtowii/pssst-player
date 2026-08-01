#include "util/ringbuf.h"
#include <string.h>
#include "util/logging.h"
#define TAG	"ringbuf"
#define RB_MASK (AUDIO_RINGBUF_FRAMES - 1)

int ring_frames_free(ring_buffer_t *rb) {
    int used = (rb->head - rb->tail) & RB_MASK;
    return AUDIO_RINGBUF_FRAMES - used - 1;
}

int ring_frames_available(ring_buffer_t *rb) {
    return (rb->head - rb->tail) & RB_MASK;
}


int ring_push(ring_buffer_t *rb, const short *frames, int count) {
    sceKernelWaitSema(rb->lock, 1, NULL);
    int n = count < ring_frames_free(rb) ? count : ring_frames_free(rb);

    int start = rb->head & RB_MASK;
    int seg = AUDIO_RINGBUF_FRAMES - start;
    if (seg > n) seg = n;
    memcpy(rb->data + start * AUDIO_CHANNELS, frames,
           seg * AUDIO_CHANNELS * sizeof(short));
    if (n > seg)
        memcpy(rb->data, frames + seg * AUDIO_CHANNELS,
               (n - seg) * AUDIO_CHANNELS * sizeof(short));

    rb->head = (rb->head + n) & RB_MASK;
    sceKernelSignalSema(rb->lock, 1);
    return n;
}

int ring_pop(ring_buffer_t *rb, short *out, int count) {
    sceKernelWaitSema(rb->lock, 1, NULL);
    int n = count < ring_frames_available(rb) ? count : ring_frames_available(rb);

    int start = rb->tail & RB_MASK;
    int seg = AUDIO_RINGBUF_FRAMES - start;
    if (seg > n) seg = n;
    memcpy(out, rb->data + start * AUDIO_CHANNELS,
           seg * AUDIO_CHANNELS * sizeof(short));
    if (n > seg)
        memcpy(out + seg * AUDIO_CHANNELS, rb->data,
               (n - seg) * AUDIO_CHANNELS * sizeof(short));

    rb->tail = (rb->tail + n) & RB_MASK;
    sceKernelSignalSema(rb->lock, 1);
    return n;
}
