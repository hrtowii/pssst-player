#ifndef UI_H
#define UI_H

#include "core/file.h"
#include "core/config.h"
#include <psptypes.h>
#include "display/clay.h"
#include "player/id3.h"

#define VISIBLE_ROWS 20
#define PSP_BUF_WIDTH	512
#define PSP_SCR_WIDTH	480
#define PSP_SCR_HEIGHT	272

extern unsigned int display_list[16384];

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} __attribute__((packed)) RGBA8888;

typedef struct {
    unsigned int color;
    short x, y, z;
} Vertex;

typedef struct {
    int width;
    int height;
    int textureWidth;
    int textureHeight;

    void *pixels;
} PSPTexture;

typedef struct {
    float u, v;
    unsigned int color;
    short x, y, z;
} TexVertex;

unsigned int color_to_gu(RGBA8888 c);

void draw_list(struct library *lib, struct config *config,
                int selected, int scroll, int playing_index);
void terminate_graphics();
void init_graphics();

void draw_rect(int x, int y, int w, int h, RGBA8888 color);
void draw_border(int x, int y, int w, int h, RGBA8888 color, Clay_BorderWidth width);
void draw_texture(int x, int y, int w, int h, PSPTexture *texture);

void set_scissor(int x, int y, int w, int h);
void clear_scissor(void);

void start_frame(void);
void end_frame(void);

typedef struct {
    struct library *lib;
    struct config  *config;
    int             selected;
    int             scroll;
    int             playing_index;

    struct ID3Tag   id3;
    PSPTexture      album_art;

    int             current_sec;
    int             total_sec;

    bool            playlist_collapsed;
} AppState;

void build_ui(AppState *app);

#endif
