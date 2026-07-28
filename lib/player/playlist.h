#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <stdbool.h>
#include <pspkernel.h>
#include "audio/audio.h"

typedef struct {
    char  *paths[AUDIO_MAX_TRACKS];
    int    count;
    int    current;
    repeat_mode_t repeat;
    bool   shuffle;
    SceUID lock;
} playlist_t;

const char *playlist_advance(playlist_t *pl);

#endif
