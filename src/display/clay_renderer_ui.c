#define CLAY_IMPLEMENTATION
#include "display/clay_renderer_ui.h"
#include "display/clay.h"
#include "display/font.h"
#include "ui.h"
#include <stdint.h>
#include <stdlib.h>
static struct {
  int width;
  int height;
} g_renderer = {0};

void init_clay() {
  uint64_t clay_mem_size = Clay_MinMemorySize();
  Clay_Arena clay_arena = Clay_CreateArenaWithCapacityAndMemory(
      clay_mem_size, malloc(clay_mem_size));
  Clay_Initialize(clay_arena, (Clay_Dimensions){480, 272},
                  (Clay_ErrorHandler){0});
  Clay_SetMeasureTextFunction(clay_measure_text, NULL);
  g_renderer.width = 480;
  g_renderer.height = 272;
};

static RGBA8888 clay_color_to_rgba(Clay_Color c) {
  return (RGBA8888){
      .r = (uint8_t)c.r,
      .g = (uint8_t)c.g,
      .b = (uint8_t)c.b,
      .a = (uint8_t)c.a,
  };
}

Clay_Dimensions clay_measure_text(Clay_StringSlice text,
                                  Clay_TextElementConfig *config,
                                  void *userData) {
  (void)config;
  (void)userData;
  int max_width = 0, line_width = 0, lines = 1;
  for (int i = 0; i < text.length; i++) {
    if (text.chars[i] == '\n') {
      if (line_width > max_width)
        max_width = line_width;
      line_width = 0;
      lines++;
    } else {
      line_width++;
    }
  }
  if (line_width > max_width)
    max_width = line_width;
  return (Clay_Dimensions){
      .width = (float)(max_width * GLYPH_W), // 8
      .height = (float)(lines * GLYPH_H),    // 16
  };
}

void clay_renderer_render(Clay_RenderCommandArray commands) {
  for (int i = 0; i < commands.length; i++) {
    Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
    Clay_BoundingBox bb = cmd->boundingBox;
    int x = (int)bb.x, y = (int)bb.y, w = (int)bb.width, h = (int)bb.height;

    switch (cmd->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      Clay_RectangleRenderData data = cmd->renderData.rectangle;
      draw_rect(x, y, w, h, clay_color_to_rgba(data.backgroundColor));
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_BORDER: {
      Clay_BorderRenderData data = cmd->renderData.border;
      // data.cornerRadius TODO
      draw_border(x, y, w, h, clay_color_to_rgba(data.color), data.width);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      Clay_TextRenderData data = cmd->renderData.text;
      draw_text8x16(x, y, data.stringContents.chars, data.stringContents.length,
                    clay_color_to_rgba(data.textColor));
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
      set_scissor(x, y, w, h);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
      clear_scissor();
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
      Clay_ImageRenderData data = cmd->renderData.image;

      PSPTexture *texture = (PSPTexture *)data.imageData;
      Clay_CornerRadius corner_radius = data.cornerRadius;

      if (texture != NULL) {
        draw_texture(x, y, w, h, texture);
      }
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
    case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
    case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
    case CLAY_RENDER_COMMAND_TYPE_NONE:
    default:
      break;
    }
  }
}
