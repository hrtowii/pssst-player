#include <pspdebug.h>
#include "ui.h"
#include "audio.h"

void draw_list(struct library *lib, struct config *config,
               int selected, int scroll, int playing_index)
{
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("Media: %s  (%zu files)\n", config->media_dir, lib->len);
    pspDebugScreenPrintf("Up/Down move  X play  O pause  []stop  L/R prev/next  Home quit\n");

    const char *state_str = "stopped";
    audio_state_t st = audio_get_state();
    if (st == AUDIO_STATE_PLAYING) state_str = "playing";
    else if (st == AUDIO_STATE_PAUSED) state_str = "paused";
    pspDebugScreenPrintf("State: %s\n\n", state_str);

    int start = scroll;
    int end   = scroll + VISIBLE_ROWS;
    if (end > (int)lib->len) end = (int)lib->len;

    for (int i = start; i < end; i++) {
        char cursor = (i == selected) ? '>' : ' ';
        char note   = (i == playing_index) ? '*' : ' ';
        pspDebugScreenPrintf("%c%c%s\n", cursor, note, lib->items[i].path);
    }
}
