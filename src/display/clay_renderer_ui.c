#if 0
#include "display/clay_renderer_ui.h"
#include "ui.h"
#include <stdint.h>

static struct {
    RGBA8888 *fb;
    int width;
    int height;
    Clay_BoundingBox scissor;
} g_renderer = {0};

void clay_renderer_init(RGBA8888 *fb, int width, int height) {
    g_renderer.fb = fb;
    g_renderer.width = width;
    g_renderer.height = height;
    g_renderer.scissor = (Clay_BoundingBox){
        .x = 0, .y = 0,
        .width = (float)width, .height = (float)height,
    };
}
void clay_renderer_render(Clay_RenderCommandArray commands) {
    if (!g_renderer.fb) return;


    for (int i = 0; i < commands.length; i++) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleRenderData data = cmd->renderData.rectangle;
                gfx_fill_rect_rgb565(g_renderer.fb, g_renderer.width, g_renderer.height,
                                     x, y, w, h,
                                     color_to_rgb565(data.backgroundColor), &clip);
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                Clay_BorderRenderData data = cmd->renderData.border;
                break;
            }

case CLAY_RENDER_COMMAND_TYPE_TEXT: {
    Clay_TextRenderData data = cmd->renderData.text;

    if (data.fontSize >= NUMFONT16X24_HEIGHT &&
        gfx_draw_textn_16x24(g_renderer.fb, g_renderer.width, g_renderer.height,
                              x, y,
                              data.stringContents.chars,
                              data.stringContents.length,
                              color, &clip);
    } else {
        gfx_draw_textn_8x16(g_renderer.fb, g_renderer.width, g_renderer.height,
                             x, y,
                             data.stringContents.chars,
                             data.stringContents.length,
                             color, &clip);
    }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                g_renderer.scissor = cmd->boundingBox;
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                g_renderer.scissor = (Clay_BoundingBox){
                    .x = 0, .y = 0,
                    .width = (float)g_renderer.width, .height = (float)g_renderer.height,
                };
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
            case CLAY_RENDER_COMMAND_TYPE_NONE:
            default:
                break;
        }
    }
}

Clay_Dimensions clay_measure_text(Clay_StringSlice text,
                                  Clay_TextElementConfig *config,
                                  void *userData) {
    (void)config;
    (void)userData;

    int max_width = 0;
    int line_width = 0;
    int lines = 1;

    for (int i = 0; i < text.length; i++) {
        if (text.chars[i] == '\n') {
            if (line_width > max_width) max_width = line_width;
            line_width = 0;
            lines++;
        } else {
            line_width++;
        }
    }
    if (line_width > max_width) max_width = line_width;

    return (Clay_Dimensions){
        .width = (float)(max_width * 8),
        .height = (float)(lines * 8),
    };
}
#endif
