#include <pspdebug.h>
#include "ui.h"
#include "audio.h"
#include <pspgu.h>
#include <pspdisplay.h>
#include <string.h>
#include "display/font.h"
#include "display/clay_renderer_ui.h"
unsigned int __attribute__((aligned(16))) display_list[16384];

unsigned int color_to_gu(RGBA8888 c)
{
    return ((unsigned int)c.a << 24) |
           ((unsigned int)c.b << 16) |
           ((unsigned int)c.g << 8)  |
           ((unsigned int)c.r);
}


void* frame_buffer_draw = NULL;
void* frame_buffer_disp = NULL;
void* depth_buffer = NULL;

void init_graphics()
{
    // 480 width * 4 bytes per pixel (GU_PSM_8888) = 1920 stride width
    frame_buffer_draw = (void*)0x00000000;                       // Starts at beginning of VRAM
    frame_buffer_disp = (void*)(1920 * 272);                     // Placed right after the draw buffer
    depth_buffer      = (void*)((1920 * 272) + (1920 * 272));    // Placed after the display buffer

    sceGuInit();

    sceGuStart(GU_DIRECT, display_list);

    sceGuDrawBuffer(GU_PSM_8888, frame_buffer_draw, 512); 
    sceGuDispBuffer(480, 272, frame_buffer_disp, 512);
    sceGuDepthBuffer(depth_buffer, 512);

    sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
    sceGuViewport(2048, 2048, 480, 272);
    sceGuScissor(0, 0, 480, 272);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);

    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplaySetFrameBuf(frame_buffer_disp, 512, GU_PSM_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
    sceGuDisplay(GU_TRUE);
}

void terminate_graphics()
{
    sceGuDisplay(GU_FALSE);
    
    sceGuTerm();
}

void draw_rect(int x, int y, int w, int h, RGBA8888 color)
{
    unsigned int gu_color = color_to_gu(color);

    Vertex* verts = (Vertex*)sceGuGetMemory(2 * sizeof(Vertex));
    verts[0].color = gu_color;
    verts[0].x = x;
    verts[0].y = y;
    verts[0].z = 0;
    verts[1].color = gu_color;
    verts[1].x = x + w;
    verts[1].y = y + h;
    verts[1].z = 0;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES,
                   GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   2, 0, verts);
}

void start_frame(void)
{
    sceGuStart(GU_DIRECT, display_list);
}

void end_frame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

// ha ha we are scissoring ha ha yuri
void set_scissor(int x, int y, int w, int h) {
    sceGuScissor(x, y, x + w, y + h);
}

void clear_scissor(void) {
    sceGuScissor(0, 0, 480, 272);
}

#define ROW_H 16       // matches GLYPH_H, one line of text per row
void list_row_component(struct library *lib, int index, bool is_selected, bool is_playing) {
    static char row_text[VISIBLE_ROWS][80];
    int slot = index % VISIBLE_ROWS;
    snprintf(row_text[slot], sizeof(row_text[slot]), "%c%c%s",
             is_selected ? '>' : ' ',
             is_playing ? '*' : ' ',
             lib->items[index].path);

    Clay_String row_str = {
        .chars = row_text[slot],
        .length = (int32_t)strlen(row_text[slot])
    };

    Clay_Color bg = is_selected
        ? (Clay_Color){ 60, 60, 90, 255 }
        : (Clay_Color){ 0, 0, 0, 255 };

    CLAY(CLAY_IDI("Row", index), (Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(ROW_H) }
        },
        .backgroundColor = bg
    }) {
        CLAY_TEXT(row_str, CLAY_TEXT_CONFIG({
            .fontSize = GLYPH_H,
            .textColor = (Clay_Color){ 255, 255, 255, 255 }
        }));
    }
}

void build_ui(struct library *lib, struct config *config, int selected, int scroll, int playing_index)
{
    CLAY(CLAY_ID("Root"), (Clay_ElementDeclaration){
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = (Clay_Color){ 0, 0, 0, 255 }
    }) {
        int start = scroll;
        int end = scroll + VISIBLE_ROWS;
        if (end > (int)lib->len) end = (int)lib->len;

        for (int i = start; i < end; i++) {
            list_row_component(lib, i, i == selected, i == playing_index);
        }
    }
}

// pspdebug printf ui
void draw_list(struct library *lib, struct config *config,
               int selected, int scroll, int playing_index)
{
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("Media: %s  (%zu files)\n", config->media_dir, lib->len);
    pspDebugScreenPrintf("Up/Down move  X play  O pause  []stop  L/R prev/next  Home quit\n");

    const char *state_str = "stopped";
    audio_state_t st = audio_get_state();
    if (st == AUDIO_STATE_PLAYING) state_str = "playing";
    else if (st == AUDIO_STATE_PAUSED) state_str = "paused";
    pspDebugScreenPrintf("State: %s\n\n", state_str);

    int start = scroll;
    int end   = scroll + VISIBLE_ROWS;
    if (end > (int)lib->len) end = (int)lib->len;

    for (int i = start; i < end; i++) {
        char cursor = (i == selected) ? '>' : ' ';
        char note   = (i == playing_index) ? '*' : ' ';
        pspDebugScreenPrintf("%c%c%s\n", cursor, note, lib->items[i].path);
    }
}
