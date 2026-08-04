#include "audio/audio.h"
#include "core/config.h"
#include "core/file.h"
#include "display/art_thread.h"
#include "display/clay_renderer_ui.h"
#include "display/font.h"
#include "display/ui.h"
#include "player/metadata.h"
#include "stdlib.h"
#include "util/defer.h"
#include "util/logging.h"
#include "util/util.h"
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <psprtc.h>
#define TAG "main"
PSP_MODULE_INFO("pssst_player", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static int exit_callback(int arg1, int arg2, void *common) {
  (void)arg1;
  (void)arg2;
  (void)common;
  sceKernelExitGame();
  return 0;
}

static int callback_thread(SceSize args, void *argp) {
  (void)args;
  (void)argp;
  int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
  sceKernelRegisterExitCallback(cbid);
  sceKernelSleepThreadCB();
  return 0;
}

static void setup_callbacks(void) {
  int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11,
                                   0xFA0, 0, NULL);
  if (thid >= 0)
    sceKernelStartThread(thid, 0, NULL);
}

static uint32_t art_generation = 0;

int main(int argc, char *argv[]) {
  setup_callbacks();

  // pspDebugScreenInit();
  log_init("ms0:/pssstdebug.log");
  defer { log_shutdown(); }
  char app_dir[MAX_PATH];
  get_app_dir(argc, argv, app_dir, sizeof(app_dir));
  char config_path[MAX_PATH];
  if (join_path(config_path, sizeof(config_path),
                "ms0:/PSP/SAVEDATA/pssst_player/", "config.txt") < 0)
    die("failed to config join");

  struct config config;
  if (config_load(config_path, &config) < 0)
    die("can't read config dir");

  struct library lib;
  if (scan_library(&lib, config.media_dir) < 0)
    die("scan library died");
  defer { scan_library_free(&lib); }

  LOG_DEBUG("SCAN", "Cached items       %zu", lib.len);

  if (!audio_init())
    die("audio_init failed");
  defer { audio_shutdown(); }

  // build the path array audio_playlist_set expects
  const char **paths = malloc(sizeof(char *) * lib.len);
  if (!paths)
    die("oom building playlist paths");
  defer { free(paths); }
  for (size_t i = 0; i < lib.len; i++)
    paths[i] = lib.items[i].path;

  if (!audio_playlist_set(paths, (int)lib.len))
    LOG_DEBUG("AUDIO", "playlist too large, truncated to %d", AUDIO_MAX_TRACKS);

  init_graphics();
  defer { terminate_graphics(); }
  init_font_texture();
  init_clay();

  if (!art_thread_init())
    LOG_ERR(TAG, "art_thread_init failed");
  defer { art_thread_shutdown(); }

  AppState app = {0};
  app.lib = &lib;
  app.config = &config;
  app.current_dir = 0;
  app.screen = SCREEN_BROWSE;

  ui_dirty.input = true;

  SceCtrlData pad, old = {0};
  while (1) {
    sceCtrlReadBufferPositive(&pad, 1);
    int pressed = pad.Buttons & ~old.Buttons;
    old = pad;

    if (pressed & PSP_CTRL_HOME)
      break;

    if (app.screen == SCREEN_BROWSE) {
      uint32_t row_count = dir_row_count(app.lib, app.current_dir);

      if (pressed & PSP_CTRL_UP) {
        if (app.selected > 0) {
          app.selected--;
          if (app.selected < app.scroll)
            app.scroll = app.selected;
          ui_dirty.input = true;
        }
      }
      if (pressed & PSP_CTRL_DOWN) {
        if (app.selected < (int)row_count - 1) {
          app.selected++;
          if (app.selected >= app.scroll + VISIBLE_ROWS)
            app.scroll = app.selected - VISIBLE_ROWS + 1;
          ui_dirty.input = true;
        }
      }

      if (pressed & PSP_CTRL_RIGHT) {
        uint32_t idx;
        if (row_count > 0 &&
            dir_row_resolve(app.lib, app.current_dir, app.selected, &idx)) {
          app.current_dir = idx;
          app.selected = 0;
          app.scroll = 0;
          ui_dirty.input = true;
        }
      }

      if (pressed & PSP_CTRL_LEFT) {
        uint32_t parent = app.lib->dirs[app.current_dir].parent;
        if (parent != ROOT_DIR) {
          uint32_t prev_dir = app.current_dir;
          app.current_dir = parent;
          app.selected = 0;
          app.scroll = 0;
          struct dir *p = &app.lib->dirs[parent];
          for (uint32_t i = 0; i < p->child_dir_len; i++) {
            if (p->child_dirs[i] == prev_dir) {
              app.selected = (int)i;
              if (app.selected >= VISIBLE_ROWS)
                app.scroll = app.selected - VISIBLE_ROWS + 1;
              break;
            }
          }
          ui_dirty.input = true;
        }
      }

      if (pressed & PSP_CTRL_CROSS) {
        uint32_t idx;
        if (row_count > 0 &&
            dir_row_resolve(app.lib, app.current_dir, app.selected, &idx)) {
          app.current_dir = idx;
          app.selected = 0;
          app.scroll = 0;
          ui_dirty.input = true;
        } else if (row_count > 0) {
          LOG_DEBUG(TAG, "playing song %u", idx);
          audio_play_index((int)idx);
          app.screen = SCREEN_NOWPLAYING;
          ui_dirty.input = true;
        }
      }
    }

    if (pressed & PSP_CTRL_CIRCLE) {
      if (audio_get_state() == AUDIO_STATE_PLAYING)
        audio_pause();
      else
        audio_play();
    }
    if (pressed & PSP_CTRL_SQUARE) {
      audio_stop();
    }
    if (pressed & PSP_CTRL_RTRIGGER) {
      audio_next();
    }
    if (pressed & PSP_CTRL_LTRIGGER) {
      audio_prev();
    }
    if (pressed & PSP_CTRL_TRIANGLE) {
      audio_set_shuffle(!audio_get_shuffle());
      ui_dirty.input = true;
    }
    if (pressed & PSP_CTRL_START) {
      repeat_mode_t next;
      switch (audio_get_repeat()) {
      case REPEAT_OFF:
        next = REPEAT_ALL;
        break;
      case REPEAT_ALL:
        next = REPEAT_ONE;
        break;
      default:
        next = REPEAT_OFF;
        break;
      }
      audio_set_repeat(next);
      ui_dirty.input = true;
    }
    if (pressed & PSP_CTRL_SELECT) {
      if (app.playing_index >= 0) {
        app.screen =
            (app.screen == SCREEN_BROWSE) ? SCREEN_NOWPLAYING : SCREEN_BROWSE;
        ui_dirty.input = true;
      }
    }

    int idx = audio_get_current_index();
    if (idx != app.playing_index) {
      app.playing_index = idx;
      ui_dirty.track = true;
      if (idx >= 0) {
        metadata_free(&app.meta);
        metadata_loader_t *loader =
            metadata_loader_for(audio_get_current_path());
        if (loader)
          loader->load(loader, audio_get_current_path(), &app.meta);
        app.total_sec = app.meta.total_seconds;
        art_submit(idx, ++art_generation, &app.meta);
      } else {
        if (app.album_art.pixels) {
          free(app.album_art.pixels);
          app.album_art = (PSPTexture){0};
        }
        if (app.bg_texture_data.pixels) {
          free(app.bg_texture_data.pixels);
          app.bg_texture_data = (PSPTexture){0};
        }
        app.has_bg_texture = false;
        art_generation++;
        app.screen = SCREEN_BROWSE;
        ui_dirty.input = true;
      }
    }

    ArtResult art;
    if (art_poll(&art)) {
      if (art.generation == art_generation) {
        if (app.album_art.pixels)
          free(app.album_art.pixels);
        if (app.bg_texture_data.pixels)
          free(app.bg_texture_data.pixels);
        app.album_art = art.album_art;
        app.bg_texture_data = art.background;
        app.has_bg_texture = app.bg_texture_data.pixels != NULL;
        ui_dirty.track = true;
      } else {
        if (art.album_art.pixels)
          free(art.album_art.pixels);
        if (art.background.pixels)
          free(art.background.pixels);
      }
    }

    int sec = audio_get_current_second();
    if (sec != app.current_sec) {
      app.current_sec = sec;
      ui_dirty.time = true;
    }

    if (ui_dirty.input || ui_dirty.time || ui_dirty.track) {
      start_frame();
      Clay_BeginLayout();
      build_ui(&app);
      Clay_RenderCommandArray cmds = Clay_EndLayout(0);
      clay_renderer_render(cmds);
      end_frame();
      ui_dirty = (UiDirty){0};
    } else {
      sceKernelDelayThread(5000);
    }
  }
  sceKernelExitGame();
  return 0;
}
