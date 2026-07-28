#include "audio.h"
#include "config.h"
#include "defer.h"
#include "display/clay_renderer_ui.h"
#include "display/font.h"
#include "display/image.h"
#include "file.h"
#include "logging.h"
#include "stdlib.h"
#include "ui.h"
#include "util.h"
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

  int selected = 0;
  int scroll = 0;

  SceCtrlData pad, old = {0};

  while (1) {
    sceCtrlReadBufferPositive(&pad, 1);
    int pressed = pad.Buttons & ~old.Buttons;
    old = pad;

    if (pressed & PSP_CTRL_HOME)
      break;

    if (pressed & PSP_CTRL_UP) {
      if (selected > 0) {
        selected--;
        if (selected < scroll)
          scroll = selected;
      }
    }
    if (pressed & PSP_CTRL_DOWN) {
      if (selected < (int)lib.len - 1) {
        selected++;
        if (selected >= scroll + VISIBLE_ROWS)
          scroll = selected - VISIBLE_ROWS + 1;
      }
    }

    if (pressed & PSP_CTRL_CROSS) {
      LOG_DEBUG(TAG, "playing song %d", selected);
      audio_play_index(selected);
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
    start_frame();

    Clay_BeginLayout();
    build_ui(&lib, &config, selected, scroll, audio_get_current_index());
    Clay_RenderCommandArray cmds = Clay_EndLayout(0);
    clay_renderer_render(cmds);

    end_frame();
    // draw_list(&lib, &config, selected, scroll, audio_get_current_index());
  }
  sceKernelExitGame();
  return 0;
}
