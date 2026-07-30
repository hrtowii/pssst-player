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

void list_row_component(struct library *lib, int index, bool is_selected,
                        bool is_playing, int row_width_px) {
  static char row_text[VISIBLE_ROWS][80];
  int slot = index % VISIBLE_ROWS;
  snprintf(row_text[slot], sizeof(row_text[slot]), "%c%c%s",
           is_selected ? '>' : ' ', is_playing ? '*' : ' ',
           path_basename(lib->items[index].path));

  int budget_px = row_width_px - 4 - (2 * GLYPH_W);
  truncate_to_width(row_text[slot], sizeof(row_text[slot]), budget_px);

  Clay_String row_str = {.chars = row_text[slot],
                         .length = (int32_t)strlen(row_text[slot])};

  Clay_Color bg = is_selected ? COLOR_ACCENT_DIM : COLOR_SURFACE;

  CLAY(CLAY_IDI("Row", index),
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

static void render_list(struct library *lib, int selected, int scroll,
                        int playing_index, int row_width_px) {
  int start = scroll;
  int end = scroll + VISIBLE_ROWS;
  if (end > (int)lib->len)
    end = (int)lib->len;
  for (int i = start; i < end; i++) {
    list_row_component(lib, i, i == selected, i == playing_index, row_width_px);
  }
}

void build_ui(AppState *app) {
  bool playing =
      app->playing_index >= 0 && app->playing_index < (int)app->lib->len;
  bool show_left = playing && !app->playlist_collapsed;

  bool has_bg = app->has_bg_texture;

  int left_panel_px = (int)(480 * 0.25f);

  CLAY(CLAY_ID("Root"),
       (Clay_ElementDeclaration){
           .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                      .layoutDirection = CLAY_TOP_TO_BOTTOM,
                      .childGap = 4,
                      .padding = CLAY_PADDING_ALL(4)},
           	      .backgroundColor = has_bg ? (Clay_Color){0} : COLOR_BACKGROUND
    }) {

    if (has_bg) {
        CLAY(CLAY_ID("Background"),
             (Clay_ElementDeclaration){
                 .layout = {
                     .sizing = {
                         CLAY_SIZING_GROW(0),
                         CLAY_SIZING_GROW(0)
                     }
                 },
                 .image = {
                     .imageData = &app->bg_texture_data
                 },
                 .floating = {
                     .attachTo = CLAY_ATTACH_TO_PARENT,
                     .attachPoints = {
                         .element = CLAY_ATTACH_POINT_LEFT_TOP,
                         .parent = CLAY_ATTACH_POINT_LEFT_TOP
                     },
                     .zIndex = -1
                 }
             }) {}
    }

    CLAY(CLAY_ID("MainRow"),
         (Clay_ElementDeclaration){
             .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childGap = 4}}) {

      if (show_left) {
        CLAY(CLAY_ID("LeftPanel"),
             (Clay_ElementDeclaration){
                 .layout = {.sizing = {.width = CLAY_SIZING_PERCENT(0.25f),
                                       .height = CLAY_SIZING_GROW(0)},
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                            .childGap = 2,
                            .padding = CLAY_PADDING_ALL(2)},
                 .backgroundColor = COLOR_BACKGROUND,
                 .border = border_all()}) {
          render_list(app->lib, app->selected, app->scroll, app->playing_index,
                      left_panel_px);
        }
      }

      CLAY(CLAY_ID("RightPanel"),
           (Clay_ElementDeclaration){
               .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                     .height = CLAY_SIZING_GROW(0)},
                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
                          .childGap = 6},
               .border =
                   show_left ? (Clay_BorderElementConfig){0} : border_all()}) {

        if (playing) {
          CLAY(CLAY_ID("NowPlayingRow"),
               (Clay_ElementDeclaration){
                   .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                         .height = CLAY_SIZING_FIT(0)},
                              .layoutDirection = CLAY_LEFT_TO_RIGHT,
                              .childGap = 4,
                              .padding = CLAY_PADDING_ALL(2)},
                   .backgroundColor = COLOR_BACKGROUND,
                   .border = show_left ? border_all()
                                       : (Clay_BorderElementConfig){0}}) {

            if (app->album_art.pixels) {
              CLAY(CLAY_ID("AlbumArt"),
                   (Clay_ElementDeclaration){
                       .layout = {.sizing = {.width = CLAY_SIZING_FIXED(80),
                                             .height = CLAY_SIZING_FIXED(80)}},
                       .image = {.imageData = &app->album_art}}) {}
            }

            CLAY(CLAY_ID("TextStack"),
                 (Clay_ElementDeclaration){
                     .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                           .height = CLAY_SIZING_GROW(0)},
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                .childGap = 2,
                                .padding = {.left = 4}}}) {

              Clay_String title_str = {.chars = app->meta.title,
                                       .length =
                                           (int32_t)strlen(app->meta.title)};
              CLAY_TEXT(title_str,
                        CLAY_TEXT_CONFIG({.fontSize = GLYPH_H,
                                          .textColor = COLOR_TEXT_PRIMARY}));

              Clay_String artist_str = {.chars = app->meta.artist,
                                        .length =
                                            (int32_t)strlen(app->meta.artist)};
              CLAY_TEXT(artist_str,
                        CLAY_TEXT_CONFIG({.fontSize = GLYPH_H,
                                          .textColor = COLOR_TEXT_SECONDARY}));

              Clay_String genre_str = {.chars = app->meta.genre,
                                       .length =
                                           (int32_t)strlen(app->meta.genre)};
              CLAY_TEXT(genre_str,
                        CLAY_TEXT_CONFIG({.fontSize = GLYPH_H,
                                          .textColor = COLOR_TEXT_MUTED}));
            }
          }

          CLAY(CLAY_ID("RightPanelSpacer"),
               (Clay_ElementDeclaration){
                   .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                         .height = CLAY_SIZING_GROW(0)}}}) {}

          CLAY(CLAY_ID("ProgressWrap"),
               (Clay_ElementDeclaration){
                   .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                         .height = CLAY_SIZING_FIT(0)},
                              .layoutDirection = CLAY_TOP_TO_BOTTOM,
                              .childGap = 2,
                              .padding = CLAY_PADDING_ALL(4),
                              .childAlignment = {.y = CLAY_ALIGN_Y_BOTTOM}},
                   .border = border_all()}) {

            CLAY(CLAY_ID("ProgressBar"),
                 (Clay_ElementDeclaration){
                     .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                           .height = CLAY_SIZING_FIXED(16)}},
                     .backgroundColor = COLOR_SURFACE_ALT}) {

              if (app->total_sec > 0) {
                float pct = (float)app->current_sec / (float)app->total_sec;
                if (pct > 1.0f)
                  pct = 1.0f;
                if (pct < 0.0f)
                  pct = 0.0f;

                CLAY(
                    CLAY_ID("ProgressFill"),
                    (Clay_ElementDeclaration){
                        .layout = {.sizing = {.width = CLAY_SIZING_PERCENT(pct),
                                              .height = CLAY_SIZING_GROW(0)}},
                        .backgroundColor = COLOR_ACCENT}) {}
              }
            }

            CLAY(CLAY_ID("ProgressTimeRow"),
                 (Clay_ElementDeclaration){
                     .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                           .height = CLAY_SIZING_FIXED(ROW_H)},
                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                .padding = {.left = 2, .right = 2}}}) {

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
              CLAY(
                  CLAY_ID("TimeSpacer"),
                  (Clay_ElementDeclaration){
                      .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                            .height = CLAY_SIZING_FIXED(1)}}}) {
              }
              CLAY_TEXT(tot_str,
                        CLAY_TEXT_CONFIG({.fontSize = GLYPH_H,
                                          .textColor = COLOR_TEXT_SECONDARY}));
            }
          }
        } else {
          render_list(app->lib, app->selected, app->scroll, app->playing_index,
                      SCREEN_W - 8);
        }
      }
    }
  }
}
