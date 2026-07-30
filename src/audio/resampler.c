#include "audio/resampler.h"
#include "util/logging.h"
#include <stdint.h>
#define RESAMPLER_FRAC_BITS 32
#define RESAMPLER_ONE (1ULL << RESAMPLER_FRAC_BITS)
#define RESAMPLER_FRAC_MASK (RESAMPLER_ONE - 1)
#define TAG	"resampler"
/*
 * fraction == 0
 *     result = a
 *
 * fraction == 0xFFFFFFFF
 *     result is almost b
 */
static int16_t resampler_lerp_s16(int16_t a, int16_t b, uint32_t fraction) {
  int32_t difference = (int32_t)b - (int32_t)a;

  int64_t offset = ((int64_t)difference * fraction) >> RESAMPLER_FRAC_BITS;

  return (int16_t)((int32_t)a + offset);
}

int resampler_init(resampler_t *resampler, int input_rate, int output_rate) {
	LOG_DEBUG(TAG, "resample initing");
  if (!resampler || input_rate <= 0 || output_rate <= 0) {
    return -1;
  }

  resampler->input_rate = input_rate;
  resampler->output_rate = output_rate;

  resampler->position = 0;

  /*
   * For every output frame, move this far through
   * the input stream.
   *
   * 48 kHz -> 44.1 kHz:
   *
   *     step = 48000 / 44100
   *          = 1.088435...
   *
   * Therefore each output frame is positioned
   * 1.088435 input frames after the previous one.
   */
  resampler->step =
      ((uint64_t)input_rate << RESAMPLER_FRAC_BITS) / (uint64_t)output_rate;

  return 0;
}

int resampler_is_passthrough(const resampler_t *resampler) {
  if (!resampler)
    return 0;

  return resampler->input_rate == resampler->output_rate;
}

int resampler_process(resampler_t *resampler, const int16_t *input,
                      int input_frames, int16_t *output, int output_capacity,
                      int *input_consumed) {
  if (input_consumed)
    *input_consumed = 0;

  if (!resampler || !input || !output || input_frames <= 0 ||
      output_capacity <= 0) {
    return 0;
  }

  if (resampler_is_passthrough(resampler)) {
    int frames = input_frames;

    if (frames > output_capacity)
      frames = output_capacity;

    for (int i = 0; i < frames * 2; i++)
      output[i] = input[i];

    if (input_consumed)
      *input_consumed = frames;

    return frames;
  }

  int output_frames = 0;

  while (output_frames < output_capacity) {
    int input_index = (int)(resampler->position >> RESAMPLER_FRAC_BITS);

    /*
     * Linear interpolation requires:
     * input[input_index]
     * input[input_index + 1]
     *
     */
    if (input_index + 1 >= input_frames)
      break;

    uint32_t fraction = (uint32_t)(resampler->position & RESAMPLER_FRAC_MASK);

    output[output_frames * 2] = resampler_lerp_s16(
        input[input_index * 2], input[(input_index + 1) * 2], fraction);

    output[output_frames * 2 + 1] = resampler_lerp_s16(
        input[input_index * 2 + 1], input[(input_index + 1) * 2 + 1], fraction);

    output_frames++;

    resampler->position += resampler->step;
  }

  int consumed = (int)(resampler->position >> RESAMPLER_FRAC_BITS);

  if (consumed >= input_frames)
    consumed = input_frames - 1;

  if (consumed < 0)
    consumed = 0;

  resampler->position -= (uint64_t)consumed << RESAMPLER_FRAC_BITS;

  if (input_consumed)
    *input_consumed = consumed;

  return output_frames;
}
