#ifndef UI_H
#define UI_H

#include "display/graphics.h"
#include "display/clay.h"
#include "core/file.h"
#include "core/config.h"
#include "player/metadata.h"

#define VISIBLE_ROWS 14

typedef struct {
    struct library *lib;
    struct config  *config;
    int             selected;
    int             scroll;
    int             playing_index;

    metadata_t      meta;
    PSPTexture      album_art;

    int             current_sec;
    int             total_sec;

    PSPTexture      bg_texture_data;
    bool            has_bg_texture;
    bool            playlist_collapsed;
} AppState;

void build_ui(AppState *app);

#endif
