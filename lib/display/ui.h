#ifndef UI_H
#define UI_H

#include "core/config.h"
#include "core/file.h"
#include "display/clay.h"
#include "display/graphics.h"
#include "player/metadata.h"

#define VISIBLE_ROWS 14
#define BG_W 24
#define BG_H 14

typedef struct {
  volatile bool input; // a button was pressed
  volatile bool time;  // playback second advanced
  volatile bool track; // current track changed
} UiDirty;
extern volatile UiDirty ui_dirty;

typedef enum {
  SCREEN_BROWSE,
  SCREEN_NOWPLAYING,
} AppScreen;

typedef struct {
  struct library *lib;
  struct config *config;
  int selected;
  int scroll;
  int playing_index;

  metadata_t meta;
  PSPTexture album_art;

  int current_sec;
  int total_sec;

  PSPTexture bg_texture_data;
  bool has_bg_texture;
  AppScreen screen;
  uint32_t current_dir;
} AppState;

void build_ui(AppState *app);
uint32_t dir_row_count(struct library *lib, uint32_t dir_idx);
bool dir_row_resolve(struct library *lib, uint32_t dir_idx, int row,
                     uint32_t *out_idx);

#endif
