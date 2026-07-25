#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include "audio.h"
#include "file.h"
#include "logging.h"
#include "config.h"
#include "util.h"
#include "defer.h"
PSP_MODULE_INFO("pssst_player", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

// sceKernelCreateThread -> name, entry, initpriority, stack size, attr, option
static void setup_callbacks(void) {
    int thid = sceKernelCreateThread("update_thread", callback_thread,
                                      0x11, 0xFA0, 0, NULL);
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}

int main(int argc, char * argv[] ){
    setup_callbacks();
    pspDebugScreenInit();
    char app_dir[MAX_PATH];
    get_app_dir(argc, argv, app_dir, sizeof(app_dir));
    struct config config;
    if (config_load(app_dir, &config) <= 0) 
	    die("can't read config dir");
    
        struct library lib;
	if (scan_library(&lib, config.media_dir) < 0)
		die("scan_library");
	LOG_DEBUG("SCAN", "Cached items       %zu", lib.len);
    // audio_init();

    SceCtrlData pad;
    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START) break;

        pspDebugScreenSetXY(0, 0);
        pspDebugScreenPrintf("pssst_player running - press START to exit");

        sceKernelDelayThread(16000);
    }

    // audio_shutdown();
    sceKernelExitGame();
    return 0;
}
