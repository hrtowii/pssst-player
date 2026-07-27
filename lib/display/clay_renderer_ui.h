#ifndef CLAY_DISPLAY_RENDERER_H
#define CLAY_DISPLAY_RENDERER_H

#include <stdint.h>
#define CLAY_IMPLEMENTATION
#include "display/clay.h"

#include "ui.h"

void clay_renderer_init(rgb565_t *fb, int width, int height);

void clay_renderer_render(Clay_RenderCommandArray commands);

Clay_Dimensions clay_measure_text(Clay_StringSlice text,
                                  Clay_TextElementConfig *config,
                                  void *userData);


#endif
