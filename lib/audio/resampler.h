#ifndef AUDIO_RESAMPLE_H
#define AUDIO_RESAMPLE_H

#include <stdint.h>
typedef struct {
  int input_rate;
  int output_rate;

  uint64_t position;

  /*
   * step = 48000 / 44100
   */
  uint64_t step;
} resampler_t;

int resampler_init(resampler_t *resampler, int input_rate, int output_rate);
int resampler_process(resampler_t *resampler, const int16_t *input,
                      int input_frames, int16_t *output, int output_capacity,
                      int *input_consumed);

int resampler_is_passthrough(const resampler_t *resampler);

#endif
