#include "player/metadata.h"
#include "util/util.h"
#include <FLAC/metadata.h>
#include <pspiofilemgr.h>
#include <stdlib.h>
#include <string.h>

static bool flac_load(metadata_loader_t *self, const char *path,
                      metadata_t *out) {
  (void)self;
  memset(out, 0, sizeof(*out));

  FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();
  // libFLAC has a pretty cool api for getting everything in one shot instead of
  // opening, closing, reopening etc. per field
  // https://xiph.org/flac/api/group__flac__metadata__level2.html
  if (!chain)
    return false;

  if (!FLAC__metadata_chain_read(chain, path)) {
    FLAC__metadata_chain_delete(chain);
    return false;
  }

  FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();
  if (!iterator) {
    FLAC__metadata_chain_delete(chain);
    return false;
  }

  FLAC__metadata_iterator_init(iterator, chain);

  do {
    FLAC__StreamMetadata *block = FLAC__metadata_iterator_get_block(iterator);
    if (!block)
      continue;

    if (block->type == FLAC__METADATA_TYPE_STREAMINFO) {
      unsigned sr = block->data.stream_info.sample_rate;
      if (sr > 0 && block->data.stream_info.total_samples > 0) {
        out->total_seconds = (int)(block->data.stream_info.total_samples / sr);
      }
    } else if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
      int n = block->data.vorbis_comment.num_comments;
      for (int i = 0; i < n; i++) {
        const char *entry =
            (const char *)block->data.vorbis_comment.comments[i].entry;
        const char *eq = strchr(entry, '=');
        if (!eq)
          continue;
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
    } else if (block->type == FLAC__METADATA_TYPE_PICTURE) {
      FLAC__StreamMetadata_Picture *pic = &block->data.picture;

      if (pic->type == FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER ||
          out->art_data == NULL) {
        if (pic->data_length > 0 && pic->data != NULL) {
          if (out->art_data) {
            free(out->art_data);
            out->art_data = NULL;
          }

          out->art_data = (unsigned char *)malloc(pic->data_length);
          if (out->art_data) {
            memcpy(out->art_data, pic->data, pic->data_length);
            out->art_len = pic->data_length;

            out->art_type =
                (int)pic->type; // TODO: this isnt being used currently as its
                                // just whatever stb image parses so eh
          }
        }
      }
    }
  } while (FLAC__metadata_iterator_next(iterator));

  FLAC__metadata_iterator_delete(iterator);
  FLAC__metadata_chain_delete(chain);

  if (!strlen(out->title))
    filename_to_title(path, out->title, sizeof(out->title));

  return true;
}

metadata_loader_t metadata_flac = {
    .load = flac_load,
    .priv = NULL,
};
