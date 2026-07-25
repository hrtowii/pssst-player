#ifndef UI_H
#define UI_H

#include "file.h"
#include "config.h"

#define VISIBLE_ROWS 20

void draw_list(struct library *lib, struct config *config,
                int selected, int scroll, int playing_index);

#endif
