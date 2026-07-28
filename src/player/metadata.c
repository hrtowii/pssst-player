#include <string.h>
#include <stdlib.h>
#include "player/metadata.h"

metadata_loader_t *metadata_loader_for(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return NULL;
    if ((ext[1] == 'm' || ext[1] == 'M') &&
        (ext[2] == 'p' || ext[2] == 'P') &&
        (ext[3] == '3')) {
        return &metadata_mp3;
    }
    return NULL;
}

void metadata_free(metadata_t *meta) {
    if (meta->art_data) {
        free(meta->art_data);
        meta->art_data = NULL;
    }
    meta->art_len = 0;
}
