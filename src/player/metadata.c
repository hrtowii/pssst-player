#include <string.h>
#include <stdlib.h>
#include "player/metadata.h"

metadata_loader_t *metadata_loader_for(const char *path)
{
    const char *ext = strrchr(path, '.');

    if (!ext || ext == path || ext[1] == '\0')
        return NULL;

    ext++;

    if (strcasecmp(ext, "mp3") == 0)
        return &metadata_mp3;

    if (strcasecmp(ext, "flac") == 0)
        return &metadata_flac;

    return NULL;
}


void metadata_free(metadata_t *meta) {
    if (meta->art_data) {
        free(meta->art_data);
        meta->art_data = NULL;
    }
    meta->art_len = 0;
}
