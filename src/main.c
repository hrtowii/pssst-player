#include "audio/audio.h"
#include "core/config.h"
#include "core/file.h"
#include "display/clay_renderer_ui.h"
#include "display/font.h"
#include "display/bg.h"
#include "display/image.h"
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
static void load_test_image(PSPTexture *texture, const char *path) {
  *texture = load_texture(path);

  if (texture->pixels == NULL)
    die("failed to load image");
}
int main(int argc, char *argv[]) {
  setup_callbacks();

  // pspDebugScreenInit();
  log_init("ms0:/PSP/SAVEDATA/pssst_player/debug.log");
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

  AppState app = {0};
  app.lib = &lib;
  app.config = &config;

  SceCtrlData pad, old = {0};
  while (1) {
    sceCtrlReadBufferPositive(&pad, 1);
    int pressed = pad.Buttons & ~old.Buttons;
    old = pad;

    if (pressed & PSP_CTRL_HOME)
      break;

    if (pressed & PSP_CTRL_UP) {
      if (app.selected > 0) {
        app.selected--;
        if (app.selected < app.scroll)
          app.scroll = app.selected;
      }
    }
    if (pressed & PSP_CTRL_DOWN) {
      if (app.selected < (int)lib.len - 1) {
        app.selected++;
        if (app.selected >= app.scroll + VISIBLE_ROWS)
          app.scroll = app.selected - VISIBLE_ROWS + 1;
      }
    }

    if (pressed & PSP_CTRL_CROSS) {
      LOG_DEBUG(TAG, "playing song %d", app.selected);
      audio_play_index(app.selected);
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
    if (pressed & PSP_CTRL_SELECT) {
      app.playlist_collapsed = !app.playlist_collapsed;
    }

    int idx = audio_get_current_index();
    if (idx != app.playing_index) {
      app.playing_index = idx;

      if (app.album_art.pixels) {
        free(app.album_art.pixels);
        app.album_art = (PSPTexture){0};
      }
      if (app.bg_texture_data.pixels) {
        free(app.bg_texture_data.pixels);
        app.bg_texture_data = (PSPTexture){0};
      }
      app.has_bg_texture = false;
      if (idx >= 0) {
        metadata_free(&app.meta);
        metadata_loader_t *loader = metadata_loader_for(audio_get_current_path());
        if (loader) loader->load(loader, audio_get_current_path(), &app.meta);
        app.total_sec = app.meta.total_seconds;

        app.album_art = album_art_from_metadata(&app.meta);
        app.bg_texture_data = build_background_texture(&app.album_art);
        app.has_bg_texture = app.bg_texture_data.pixels != NULL;
      }
    }

    app.current_sec = audio_get_current_second();
    start_frame();
    Clay_BeginLayout();
    build_ui(&app);
    Clay_RenderCommandArray cmds = Clay_EndLayout(0);
    clay_renderer_render(cmds);

    end_frame();
  }
  sceKernelExitGame();
  return 0;
}
