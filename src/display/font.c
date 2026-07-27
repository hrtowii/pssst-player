#include "ui.h"
#include "display/font.h"
#include "display/8x16_font.h"
#include <stdint.h>
#include <pspgu.h>
// imma be fr i hate doing font rendering like End Me Please .
static unsigned int __attribute__((aligned(16))) font_atlas[ATLAS_W * ATLAS_H];

void build_font_atlas(const uint8_t font8x16[96][16])
{
    for (int glyph = 0; glyph < 96; glyph++) {
        int cell_x = (glyph % ATLAS_COLS) * GLYPH_W;
        int cell_y = (glyph / ATLAS_COLS) * GLYPH_H;

        for (int row = 0; row < GLYPH_H; row++) {
            uint8_t bits = font8x16[glyph][row];
            for (int col = 0; col < GLYPH_W; col++) {
                int set = (bits >> (7 - col)) & 1;
                unsigned int px = set ? 0xFFFFFFFF : 0x00000000; // white or transparent
                font_atlas[(cell_y + row) * ATLAS_W + (cell_x + col)] = px;
            }
        }
    }
}

void init_font_texture(void)
{
    build_font_atlas(FONT8X16);

    sceGuStart(GU_DIRECT, display_list);
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(0, ATLAS_W, ATLAS_H, ATLAS_W, font_atlas);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuFinish();
    sceGuSync(0, 0);
}

void draw_glyph(int x, int y, unsigned char c, RGBA8888 color)
{
	    if (c < 32 || c > 32 + 95) return;
    unsigned int gu_color = color_to_gu(color);
    unsigned char index = c - 32;
    int cell_x = (index % ATLAS_COLS) * GLYPH_W;
    int cell_y = (index / ATLAS_COLS) * GLYPH_H;

    TexVertex* v = (TexVertex*)sceGuGetMemory(2 * sizeof(TexVertex));
    v[0].u = (float)cell_x;
    v[0].v = (float)cell_y;
    v[0].color = gu_color;
    v[0].x = x; v[0].y = y; v[0].z = 0;
// this is some bullshit math that i wish i could understand like i watchd a 20 min video and gave up
    v[1].u = (float)(cell_x + GLYPH_W);
    v[1].v = (float)(cell_y + GLYPH_H);
    v[1].color = gu_color;
    v[1].x = x + GLYPH_W; v[1].y = y + GLYPH_H; v[1].z = 0;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   2, 0, v);
}

void draw_text8x16(int x, int y, const char *str, int len, RGBA8888 color) {
    unsigned int gu_color = color_to_gu(color);
    TexVertex* v = (TexVertex*)sceGuGetMemory(2 * len * sizeof(TexVertex));

    int cursor_x = x;
    int vi = 0;
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c == '\n') { cursor_x = x; y += GLYPH_H; continue; }
        if (c < 32 || c > 32 + 95) { cursor_x += GLYPH_W; continue; }

        unsigned char index = c - 32;
        int cell_x = (index % ATLAS_COLS) * GLYPH_W;
        int cell_y = (index / ATLAS_COLS) * GLYPH_H;

        v[vi].u = (float)cell_x;         v[vi].v = (float)cell_y;
        v[vi].color = gu_color;
        v[vi].x = cursor_x; v[vi].y = y; v[vi].z = 0;
        vi++;

        v[vi].u = (float)(cell_x + GLYPH_W); v[vi].v = (float)(cell_y + GLYPH_H);
        v[vi].color = gu_color;
        v[vi].x = cursor_x + GLYPH_W; v[vi].y = y + GLYPH_H; v[vi].z = 0;
        vi++;

        cursor_x += GLYPH_W;
    }

    if (vi > 0) {
        sceGuEnable(GU_TEXTURE_2D);
        sceGuDrawArray(GU_SPRITES,
                       GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                       vi, 0, v);
    }
}
