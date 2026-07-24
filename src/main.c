#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>

#include "audio.h"

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

int main(void) {
    setup_callbacks();
    pspDebugScreenInit();

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
