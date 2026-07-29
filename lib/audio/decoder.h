#ifndef DECODER_H
#define DECODER_H

#include <stdbool.h>

typedef struct decoder decoder_t;
// both the metadata and decoder struct follow a fptr pattern so that my life becomes easier for metadata parsing flac and playing it 
struct decoder {
    bool (*open)(decoder_t *self, const char *path);
    int  (*read_pcm)(decoder_t *self, short *buf, int frames);
    void (*close)(decoder_t *self);
    void *priv;
};

extern decoder_t decoder_mp3;
extern decoder_t decoder_flac;

#endif
