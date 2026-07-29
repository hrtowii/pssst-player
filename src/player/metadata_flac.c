#include <string.h>
#include <stdlib.h>
#include <pspiofilemgr.h>
#include <FLAC/metadata.h>
#include "player/metadata.h"

static void filename_to_title(const char *path, char *out, size_t outsz) {
    const char *start = strrchr(path, '/');
    if (!start) start = strrchr(path, '\\');
    if (!start) start = path; else start++;
    const char *dot = strrchr(start, '.');
    size_t n;
    if (dot) n = dot - start; else n = strlen(start);
    if (n >= outsz) n = outsz - 1;
    memcpy(out, start, n);
    out[n] = '\0';
}

static int parse_flac_duration(const char *path) {
    FLAC__StreamMetadata streaminfo;
    if (!FLAC__metadata_get_streaminfo(path, &streaminfo))
        return 0;
    unsigned sr = streaminfo.data.stream_info.sample_rate;
    if (sr == 0) return 0;
    if (streaminfo.data.stream_info.total_samples == 0)
        return 0;
    return (int)(streaminfo.data.stream_info.total_samples / sr);
}

static bool flac_load(metadata_loader_t *self, const char *path, metadata_t *out) {
    (void)self;
    memset(out, 0, sizeof(*out));

    FLAC__StreamMetadata *info = NULL;
    if (FLAC__metadata_get_tags(path, &info)) {
        if (info->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
            int n = info->data.vorbis_comment.num_comments;
            for (int i = 0; i < n; i++) {
                const char *entry = (const char *)info->data.vorbis_comment.comments[i].entry;
                const char *eq = strchr(entry, '=');
                if (!eq) continue;
                size_t klen = eq - entry;
                const char *val = eq + 1;

                if (klen == 5 && !strncasecmp(entry, "TITLE", 5))
                    strncpy(out->title, val, sizeof(out->title) - 1);
                else if (klen == 6 && !strncasecmp(entry, "ARTIST", 6))
                    strncpy(out->artist, val, sizeof(out->artist) - 1);
                else if (klen == 5 && !strncasecmp(entry, "ALBUM", 5))
                    strncpy(out->album, val, sizeof(out->album) - 1);
                else if (klen == 5 && !strncasecmp(entry, "GENRE", 5))
                    strncpy(out->genre, val, META_GENRE_LEN - 1);
                else if ((klen == 4 && !strncasecmp(entry, "DATE", 4)) ||
                         (klen == 4 && !strncasecmp(entry, "YEAR", 4)))
                    strncpy(out->year, val, META_YEAR_LEN - 1);
                else if (klen == 11 && !strncasecmp(entry, "TRACKNUMBER", 11))
                    out->track = atoi(val);
            }
        }
        FLAC__metadata_object_delete(info);
    }

    if (!strlen(out->title))
        filename_to_title(path, out->title, sizeof(out->title));

    out->total_seconds = parse_flac_duration(path);
    return true;
}

metadata_loader_t metadata_flac = {
    .load = flac_load,
    .priv = NULL,
};
