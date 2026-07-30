#define BG_W 24
#define BG_H 14
#include <stdint.h>
#include "display/ui.h"
#include "display/image.h"
#include "util/logging.h"

#define TAG	"bg"
PSPTexture build_background_texture(const PSPTexture *art)
{
  if (!art || !art->pixels || art->width <= 0 || art->height <= 0)
    return (PSPTexture){0};

  const RGBA8888 *src = (const RGBA8888 *)art->pixels;
  int sw     = art->width;          // valid content width
  int sh     = art->height;         // valid content height
  int stride = art->textureWidth;   // actual row stride in the buffer (POT-padded)
  if (stride <= 0) stride = sw;     // fallback if texture wasn't padded

  RGBA8888 tmp[BG_W * BG_H];

  for (int y = 0; y < BG_H; y++) {
    for (int x = 0; x < BG_W; x++) {
      int sx0 = (x * sw) / BG_W,     sx1 = ((x + 1) * sw) / BG_W;
      int sy0 = (y * sh) / BG_H,     sy1 = ((y + 1) * sh) / BG_H;
      if (sx1 <= sx0) sx1 = sx0 + 1;
      if (sy1 <= sy0) sy1 = sy0 + 1;

      long r = 0, g = 0, b = 0, n = 0;
      for (int sy = sy0; sy < sy1 && sy < sh; sy++) {
        for (int sx = sx0; sx < sx1 && sx < sw; sx++) {
          const RGBA8888 *px = &src[sy * stride + sx];
          r += px->r; g += px->g; b += px->b; n++;
        }
      }
      if (n == 0) n = 1;

      const int dim_pct = 60; // keep 60% brightness so text stays readable
      RGBA8888 *dst = &tmp[y * BG_W + x];
      dst->r = (uint8_t)((r / n) * dim_pct / 100);
      dst->g = (uint8_t)((g / n) * dim_pct / 100);
      dst->b = (uint8_t)((b / n) * dim_pct / 100);
      dst->a = 255;
    }
  }

  RGBA8888 blurred[BG_W * BG_H];
  for (int y = 0; y < BG_H; y++) {
    for (int x = 0; x < BG_W; x++) {
      int r = 0, g = 0, b = 0, n = 0;
      for (int dy = -1; dy <= 1; dy++) {
        int ny = y + dy; if (ny < 0 || ny >= BG_H) continue;
        for (int dx = -1; dx <= 1; dx++) {
          int nx = x + dx; if (nx < 0 || nx >= BG_W) continue;
          const RGBA8888 *px = &tmp[ny * BG_W + nx];
          r += px->r; g += px->g; b += px->b; n++;
        }
      }
      RGBA8888 *dst = &blurred[y * BG_W + x];
      dst->r = (uint8_t)(r / n);
      dst->g = (uint8_t)(g / n);
      dst->b = (uint8_t)(b / n);
      dst->a = 255;
    }
  }
  LOG_DEBUG(TAG, "created the texture for background");
  return create_texture_from_rgba((unsigned char *)blurred, BG_W, BG_H);
}
