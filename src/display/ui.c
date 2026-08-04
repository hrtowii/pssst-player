#include "display/ui.h"
#include "audio/audio.h"
#include "display/clay.h"
#include "display/clay_renderer_ui.h"
#include "display/colors.h"
#include "display/font.h"
#include "display/image.h"
#include "util/util.h"
#include <string.h>

#define ROW_H 16

volatile UiDirty ui_dirty = {0};

static inline Clay_BorderElementConfig border_all(void) {
  return (Clay_BorderElementConfig){.color = COLOR_BORDER,
                                    .width = {.left = BORDER_W,
                                              .right = BORDER_W,
                                              .top = BORDER_W,
                                              .bottom = BORDER_W,
                                              .betweenChildren = 0}};
}

static void format_time(int seconds, char *buf, size_t buflen) {
  if (seconds < 0)
    seconds = 0;
  snprintf(buf, buflen, "%d:%02d", seconds / 60, seconds % 60);
}

static void truncate_to_width(char *buf, size_t buflen, int max_px) {
  int max_chars = max_px / GLYPH_W;
  if (max_chars < 4)
    max_chars = 4;
  int len = (int)strlen(buf);
  if (len <= max_chars)
    return;
  int cut = max_chars - 3;
  if (cut < 0)
    cut = 0;
  if ((size_t)(cut + 4) > buflen)
    cut = (int)buflen - 4;
  snprintf(buf + cut, buflen - cut, "...");
}

uint32_t dir_row_count(struct library *lib, uint32_t dir_idx)
{
  struct dir *d = &lib->dirs[dir_idx];
  return (uint32_t)(d->child_dir_len + d->child_item_len);
}

bool dir_row_resolve(struct library *lib, uint32_t dir_idx, int row,
                     uint32_t *out_idx) {
  struct dir *d = &lib->dirs[dir_idx];
  if ((uint32_t)row < d->child_dir_len) {
    *out_idx = d->child_dirs[row];
    return true;
  }
  *out_idx = d->child_items[row - d->child_dir_len];
  return false;
}

static void list_row_component(struct library *lib, uint32_t dir_idx, int row,
                               bool is_selected, bool is_playing,
                               int row_width_px) {
  static char row_text[VISIBLE_ROWS][80];
  int slot = row % VISIBLE_ROWS;

  uint32_t idx;
  bool is_folder = dir_row_resolve(lib, dir_idx, row, &idx);

  const char *name = is_folder ? lib->dirs[idx].name : lib->items[idx].name;

  snprintf(row_text[slot], sizeof(row_text[slot]), "%c%c%c%s",
           is_selected ? '>' : ' ', is_playing ? '*' : ' ',
           is_folder ? '/' : ' ', name);

  int budget_px = row_width_px - 4 - (3 * GLYPH_W);
  truncate_to_width(row_text[slot], sizeof(row_text[slot]), budget_px);

  Clay_String row_str = {.chars = row_text[slot],
                         .length = (int32_t)strlen(row_text[slot])};

  Clay_Color bg = is_selected ? COLOR_ACCENT_DIM : COLOR_SURFACE;

  CLAY(CLAY_IDI("Row", row),
       (Clay_ElementDeclaration){
           .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                 .height = CLAY_SIZING_FIXED(ROW_H)},
                      .padding = {.left = 2, .right = 2}},
           .backgroundColor = bg,
           .clip = {.horizontal = true, .vertical = true}}) {
    CLAY_TEXT(row_str, CLAY_TEXT_CONFIG({.fontSize = GLYPH_H,
                                         .textColor = COLOR_TEXT_PRIMARY}));
  }
}

static void render_list(struct library *lib, uint32_t dir_idx, int selected,
                        int scroll, int playing_index, int row_width_px) {
  uint32_t total = dir_row_count(lib, dir_idx);
  int start = scroll;
  int end = scroll + VISIBLE_ROWS;
  if (end > (int)total)
    end = (int)total;

  for (int row = start; row < end; row++) {
    uint32_t idx;
    bool is_folder = dir_row_resolve(lib, dir_idx, row, &idx);
    bool is_playing = !is_folder && ((int)idx == playing_index);
    list_row_component(lib, dir_idx, row, row == selected, is_playing,
                       row_width_px);
  }
}

static void build_browse_screen(AppState *app) {
  CLAY(CLAY_ID("BrowsePanel"),
       (Clay_ElementDeclaration){
           .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                      .layoutDirection = CLAY_TOP_TO_BOTTOM,
                      .childGap = 2,
                      .padding = CLAY_PADDING_ALL(2)},
           .backgroundColor = COLOR_BACKGROUND,
           .border = border_all()}) {
    render_list(app->lib, app->current_dir, app->selected, app->scroll,
                app->playing_index, SCREEN_W - 8);
  }
}
static void build_nowplaying_screen(AppState *app) {
  CLAY(CLAY_ID("NowPlayingRoot"),
       (Clay_ElementDeclaration){
           .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                      .layoutDirection = CLAY_TOP_TO_BOTTOM,
                      .childGap = 4,
                      .padding = CLAY_PADDING_ALL(4)}}) {

    // row: image + title/artist
    CLAY(CLAY_ID("TopRow"),
         (Clay_ElementDeclaration){
             .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                   .height = CLAY_SIZING_GROW(0)},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childGap = 6}}) {

      CLAY(CLAY_ID("ArtPanel"),
           (Clay_ElementDeclaration){
               .layout = {.sizing = {.width = CLAY_SIZING_PERCENT(0.333f),
                                     .height = CLAY_SIZING_GROW(0)},
                          .childAlignment = {.x = CLAY_ALIGN_X_CENTER,
                                             .y = CLAY_ALIGN_Y_CENTER}}}) {
        if (app->album_art.pixels) {
          CLAY(CLAY_ID("AlbumArt"),
               (Clay_ElementDeclaration){
                   .layout = {.sizing = {.width = CLAY_SIZING_FIXED(140),
                                         .height = CLAY_SIZING_FIXED(140)}},
                   .image = {.imageData = &app->album_art}}) {}
        }
      }

      CLAY(CLAY_ID("TitlePanel"),
           (Clay_ElementDeclaration){
               .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                     .height = CLAY_SIZING_GROW(0)},
                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
                          .childGap = 2,
                          .padding = CLAY_PADDING_ALL(4)},
               .clip = {.horizontal = true}}) {
        Clay_String title_str = {.chars = app->meta.title,
                                 .length = (int32_t)strlen(app->meta.title)};
        CLAY_TEXT(title_str,
                  CLAY_TEXT_CONFIG(
                      {.fontSize = GLYPH_H, .textColor = COLOR_TEXT_PRIMARY}));

        static char artist_album[120];
        snprintf(artist_album, sizeof(artist_album), "%s - %s",
                 app->meta.artist, app->meta.album);
        Clay_String aa_str = {.chars = artist_album,
                              .length = (int32_t)strlen(artist_album)};
        CLAY_TEXT(aa_str,
                  CLAY_TEXT_CONFIG({.fontSize = GLYPH_H,
                                    .textColor = COLOR_TEXT_SECONDARY}));
      }
    }

    // track progress
    CLAY(CLAY_ID("ProgressWrap"),
         (Clay_ElementDeclaration){
             .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap = 2}}) {
      CLAY(CLAY_ID("ProgressBar"),
           (Clay_ElementDeclaration){
               .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                     CLAY_SIZING_FIXED(12)}},
               .backgroundColor = COLOR_SURFACE_ALT}) {
        if (app->total_sec > 0) {
          float pct = (float)app->current_sec / (float)app->total_sec;
          if (pct > 1.0f) pct = 1.0f;
          if (pct < 0.0f) pct = 0.0f;
          CLAY(CLAY_ID("ProgressFill"),
               (Clay_ElementDeclaration){
                   .layout = {.sizing = {.width = CLAY_SIZING_PERCENT(pct),
                                         .height = CLAY_SIZING_GROW(0)}},
                   .backgroundColor = COLOR_ACCENT}) {}
        }
      }
      CLAY(CLAY_ID("ProgressTimeRow"),
           (Clay_ElementDeclaration){
               .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                     CLAY_SIZING_FIXED(ROW_H)},
                          .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
        static char cur_buf[16], tot_buf[16];
        format_time(app->current_sec, cur_buf, sizeof(cur_buf));
        format_time(app->total_sec, tot_buf, sizeof(tot_buf));
        Clay_String cur_str = {.chars = cur_buf,
                               .length = (int32_t)strlen(cur_buf)};
        Clay_String tot_str = {.chars = tot_buf,
                               .length = (int32_t)strlen(tot_buf)};
        CLAY_TEXT(cur_str,
                  CLAY_TEXT_CONFIG({.fontSize = GLYPH_H,
                                    .textColor = COLOR_TEXT_SECONDARY}));
        CLAY(CLAY_ID("TimeSpacer"),
             (Clay_ElementDeclaration){
                 .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                       CLAY_SIZING_FIXED(1)}}}) {}
        CLAY_TEXT(tot_str,
                  CLAY_TEXT_CONFIG({.fontSize = GLYPH_H,
                                    .textColor = COLOR_TEXT_SECONDARY}));
      }
    }

    // row: shuffle + repeat
    CLAY(CLAY_ID("StatusRow"),
         (Clay_ElementDeclaration){
             .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childGap = 4}}) {

      bool shuffle_on = audio_get_shuffle();
      CLAY(CLAY_ID("ShuffleBadge"),
           (Clay_ElementDeclaration){
               .layout = {.padding = {.left = 4, .right = 4, .top = 1, .bottom = 1},
                          .sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}},
               .backgroundColor = shuffle_on ? COLOR_ACCENT : COLOR_SURFACE_ALT,
               .border = border_all()}) {
        Clay_String s = {.chars = "shuffle", .length = 7};
        CLAY_TEXT(s, CLAY_TEXT_CONFIG(
                          {.fontSize = GLYPH_H,
                           .textColor = shuffle_on ? COLOR_BACKGROUND
                                                   : COLOR_TEXT_MUTED}));
      }

      repeat_mode_t rep = audio_get_repeat();
      bool repeat_on = rep != REPEAT_OFF;
      static char repeat_buf[12];
      snprintf(repeat_buf, sizeof(repeat_buf), "repeat %s",
               rep == REPEAT_ONE ? "loop" : rep == REPEAT_ALL ? "all" : "off");
      Clay_String r = {.chars = repeat_buf,
                       .length = (int32_t)strlen(repeat_buf)};
      CLAY(CLAY_ID("RepeatBadge"),
           (Clay_ElementDeclaration){
               .layout = {.padding = {.left = 4, .right = 4, .top = 1, .bottom = 1},
                          .sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}},
               .backgroundColor = repeat_on ? COLOR_ACCENT : COLOR_SURFACE_ALT,
               .border = border_all()}) {
        CLAY_TEXT(r, CLAY_TEXT_CONFIG(
                          {.fontSize = GLYPH_H,
                           .textColor = repeat_on ? COLOR_BACKGROUND
                                                  : COLOR_TEXT_MUTED}));
      }

      CLAY(CLAY_ID("StatusSpacer"),
           (Clay_ElementDeclaration){
               .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                     CLAY_SIZING_FIXED(1)}}}) {}
    }

    // control map
    // CLAY(CLAY_ID("ControlsMap"),
    //      (Clay_ElementDeclaration){
    //          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
    //                     .padding = CLAY_PADDING_ALL(2)},
    //          .border = border_all()}) {
    //   static const char controls_buf[] =
    //       "[O] Pause [L/R] Skip [T] Shuffle [Start] Repeat [Select] Menu";
    //   Clay_String cm_str = {.chars = controls_buf,
    //                         .length = (int32_t)strlen(controls_buf)};
    //   CLAY_TEXT(cm_str, CLAY_TEXT_CONFIG({.fontSize = GLYPH_H,
    //                                       .textColor = COLOR_TEXT_MUTED}));
    // }
  }
}
void build_ui(AppState *app) {
  CLAY(CLAY_ID("Root"),
       (Clay_ElementDeclaration){
           .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                      .layoutDirection = CLAY_TOP_TO_BOTTOM,
                      .padding = CLAY_PADDING_ALL(4)},
           .backgroundColor =
               app->has_bg_texture ? (Clay_Color){0} : COLOR_BACKGROUND}) {

    if (app->has_bg_texture) {
      CLAY(CLAY_ID("Background"),
           (Clay_ElementDeclaration){
               .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
               .image = {.imageData = &app->bg_texture_data},
               .floating = {
                   .attachTo = CLAY_ATTACH_TO_PARENT,
                   .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                    .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                   .zIndex = -1}}) {}
    }

    if (app->screen == SCREEN_BROWSE)
      build_browse_screen(app);
    else
      build_nowplaying_screen(app);
  }
}
