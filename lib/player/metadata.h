#ifndef METADATA_H
#define METADATA_H

#include "util/util.h"
#include <stddef.h>
#include <stdbool.h>

#define META_YEAR_LEN   16
#define META_GENRE_LEN  64

typedef struct {
    char title[MAX_PATH];
    char artist[MAX_PATH];
    char album[MAX_PATH];
    char year[META_YEAR_LEN];
    char genre[META_GENRE_LEN];
    int  track;
    int  total_seconds;
    unsigned char *art_data;
    size_t art_len;
    int art_type;
} metadata_t;

typedef struct metadata_loader metadata_loader_t;
// both the metadata and decoder struct follow a fptr pattern so that my life becomes easier for metadata parsing flac and playing it 
struct metadata_loader {
    bool (*load)(metadata_loader_t *self, const char *path, metadata_t *out);
    void *priv;
};

extern metadata_loader_t metadata_mp3;
extern metadata_loader_t metadata_flac;

metadata_loader_t *metadata_loader_for(const char *path);
void metadata_free(metadata_t *meta);

#endif
