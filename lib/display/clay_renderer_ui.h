#ifndef CLAY_DISPLAY_RENDERER_H
#define CLAY_DISPLAY_RENDERER_H

#include <stdint.h>
#include "display/clay.h"

#include "display/graphics.h"

void init_clay();

void clay_renderer_render(Clay_RenderCommandArray commands);

Clay_Dimensions clay_measure_text(Clay_StringSlice text,
                                  Clay_TextElementConfig *config,
                                  void *userData);


#endif
