#include "display/graphics.h"
#define GLYPH_W 8
#define GLYPH_H 16
#define ATLAS_COLS 16
#define ATLAS_ROWS 16
#define ATLAS_W (GLYPH_W * ATLAS_COLS)  // 128
#define ATLAS_H (GLYPH_H * ATLAS_ROWS)  // 256

void init_font_texture(void);
void draw_glyph(int x, int y, unsigned char c, RGBA8888 color);
void draw_text8x16(int x, int y, const char *str, int len, RGBA8888 color);
