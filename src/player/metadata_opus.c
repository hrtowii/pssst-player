#include "player/metadata.h"
#include "util/defer.h"
#include "util/logging.h"
#include "util/util.h"
#include <opus/opusfile.h>
#include <pspiofilemgr.h>
#include <stdlib.h>
#include <string.h>
#define TAG "opus_metadata"

static void safe_copy(char *dst, const char *src, size_t dst_size) {
  size_t n = strlen(src);
  if (n >= dst_size) n = dst_size - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

static void parse_vorbis_comments(const OpusTags *tags, metadata_t *out) {
  for (int i = 0; i < tags->comments; i++) {
    char *comment = tags->user_comments[i];
    int len = tags->comment_lengths[i];

    char *eq = memchr(comment, '=', len);
    if (!eq)
      continue;

    size_t key_len = eq - comment;
    char *value = eq + 1;

    if (strncasecmp(comment, "TITLE", key_len) == 0) {
      safe_copy(out->title, value, MAX_PATH);
    } else if (strncasecmp(comment, "ARTIST", key_len) == 0) {
      safe_copy(out->artist, value, MAX_PATH);
    } else if (strncasecmp(comment, "ALBUM", key_len) == 0) {
      safe_copy(out->album, value, MAX_PATH);
    } else if (strncasecmp(comment, "DATE", key_len) == 0 ||
               strncasecmp(comment, "YEAR", key_len) == 0) {
      safe_copy(out->year, value, META_YEAR_LEN);
    } else if (strncasecmp(comment, "GENRE", key_len) == 0) {
      safe_copy(out->genre, value, META_GENRE_LEN);
    } else if (strncasecmp(comment, "TRACKNUMBER", key_len) == 0) {
      out->track = atoi(value);
    }
  }
}

static void parse_embedded_artwork(const OpusTags *tags, metadata_t *out) {
  for (int i = 0; i < tags->comments; i++) {
    char *comment = tags->user_comments[i];
    int len = tags->comment_lengths[i];
    char *eq = memchr(comment, '=', len);
    if (!eq)
      continue;

    size_t key_len = eq - comment;
    if (strncasecmp(comment, "METADATA_BLOCK_PICTURE", key_len) == 0) {
      OpusPictureTag pic;
      if (opus_picture_tag_parse(&pic, comment) == 0) {
        if (out->art_data == NULL || pic.type == 3) {
          if (out->art_data) {
            free(out->art_data);
          }

          out->art_data = (unsigned char *)malloc(pic.data_length);
          if (out->art_data) {
            memcpy(out->art_data, pic.data, pic.data_length);
            out->art_len = pic.data_length;
            out->art_type = pic.type;
          }
        }
        opus_picture_tag_clear(&pic);

        if (out->art_type == 3)
          break;
      }
    }
  }
}

static bool opus_load(metadata_loader_t *self, const char *path,
                      metadata_t *out) {
  if (!path || !out)
    return false;
   memset(out, 0, sizeof(*out));
  out->sample_rate = 48000;   // we dont care ab what the actual one is

  int err = 0;
  OggOpusFile *fd = op_open_file(path, &err);
  if (!fd || err < 0) {
    LOG_ERR(TAG, "failed to open file errno %d", err);
    return false;
  }
  ogg_int64_t pcm_samples = op_pcm_total(fd, -1);
  if (pcm_samples != OP_EINVAL) {
    out->total_seconds = (int)(pcm_samples / 48000);
  }
  const OpusTags *tags = op_tags(fd, -1);
  if (tags) {
    parse_vorbis_comments(tags, out);
    parse_embedded_artwork(tags, out);
  }
  op_free(fd);
  return true;
}

metadata_loader_t metadata_opus = {
    .load = opus_load,
    .priv = NULL,
};
