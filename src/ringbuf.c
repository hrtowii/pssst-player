#include "ringbuf.h"

int ring_frames_free(ring_buffer_t *rb) {
    int used = (rb->head - rb->tail + AUDIO_RINGBUF_FRAMES) % AUDIO_RINGBUF_FRAMES;
    return AUDIO_RINGBUF_FRAMES - used - 1; // -1 keeps head!=tail on full
}

int ring_frames_available(ring_buffer_t *rb) {
    return (rb->head - rb->tail + AUDIO_RINGBUF_FRAMES) % AUDIO_RINGBUF_FRAMES;
}

int ring_push(ring_buffer_t *rb, const short *frames, int count) {
    sceKernelWaitSema(rb->lock, 1, NULL);
    int n = count < ring_frames_free(rb) ? count : ring_frames_free(rb);
    for (int i = 0; i < n; i++) {
        int idx = (rb->head + i) % AUDIO_RINGBUF_FRAMES;
        rb->data[idx * AUDIO_CHANNELS]     = frames[i * AUDIO_CHANNELS];
        rb->data[idx * AUDIO_CHANNELS + 1] = frames[i * AUDIO_CHANNELS + 1];
    }
    rb->head = (rb->head + n) % AUDIO_RINGBUF_FRAMES;
    sceKernelSignalSema(rb->lock, 1);
    return n;
}

int ring_pop(ring_buffer_t *rb, short *out, int count) {
    sceKernelWaitSema(rb->lock, 1, NULL);
    int n = count < ring_frames_available(rb) ? count : ring_frames_available(rb);
    for (int i = 0; i < n; i++) {
        int idx = (rb->tail + i) % AUDIO_RINGBUF_FRAMES;
        out[i * AUDIO_CHANNELS]     = rb->data[idx * AUDIO_CHANNELS];
        out[i * AUDIO_CHANNELS + 1] = rb->data[idx * AUDIO_CHANNELS + 1];
    }
    rb->tail = (rb->tail + n) % AUDIO_RINGBUF_FRAMES;
    sceKernelSignalSema(rb->lock, 1);
    return n;
}
