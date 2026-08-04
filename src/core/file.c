#include "core/file.h"
#include "util/logging.h"
#include "util/util.h"
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#define TAG "file_scanner"

static uint32_t fnv1a32(const void *data, size_t length) {
  uint32_t hash = 0x811c9dc5;
  const uint8_t *p = (const uint8_t *)data;
  const uint8_t *end = p + length;

  while (p < end) {
    uint8_t c = *p++;
    // FILE.MP3 == file.mp3, dont rehash
    if (c >= 'A' && c <= 'Z') {
      c |= 0x20;
    }

    hash ^= c;
    hash *= 0x01000193;
  }

  return hash;
}

static int is_supported_media(const char *path) {
  const char *ext = strrchr(path, '.');

  if (!ext || ext == path || ext[1] == '\0')
    return 0;

  ext++;

  if (strcasecmp(ext, "mp3") == 0)
    return 1;

  if (strcasecmp(ext, "flac") == 0)
    return 1;
  // TODO find a way to register file formats more easily bc rn i have to touch
  // like 6 files
  if (strcasecmp(ext, "opus") == 0)
    return 1;

  return 0;
}

// Oh my dear, why must you scheme with me?
static int
dir_add_child_dir(struct dir *parent,
                  uint32_t child_idx) // child dir contains another child dir
{
  if (parent->child_dir_len == parent->child_dir_cap) {
    // we r full

    size_t new_cap;

    if (parent->child_dir_cap == 0) {
      // inited w 0? first child
      new_cap = 4;
    } else {
      new_cap =
          parent->child_dir_cap * 2; // shld prolly cap at some arbitrary amt
    }

    uint32_t *new_child_dirs =
        realloc(parent->child_dirs, new_cap * sizeof(*parent->child_dirs));

    if (!new_child_dirs) {
      LOG_ERR(TAG, "realloc FAILED for child dirs");
      return -1;
    }

    parent->child_dirs = new_child_dirs;
    parent->child_dir_cap = new_cap;
  }
  // the newest one is now the length
  parent->child_dirs[parent->child_dir_len] = child_idx;

  parent->child_dir_len++;

  return 0;
}

static int dir_add_child_item(struct dir *parent, uint32_t item_idx) {
  if (parent->child_item_len == parent->child_item_cap) {

    size_t new_cap;

    if (parent->child_item_cap == 0) {
      new_cap = 8;
    } else {
      new_cap = parent->child_item_cap * 2;
    }

    uint32_t *new_child_items =
        realloc(parent->child_items, new_cap * sizeof(*parent->child_items));

    if (!new_child_items) {
      LOG_ERR(TAG, "realloc FAILED for child items");
      return -1;
    }

    parent->child_items = new_child_items;
    parent->child_item_cap = new_cap;
  }

  parent->child_items[parent->child_item_len] = item_idx;
  parent->child_item_len++;

  return 0;
}

static int dir_push(struct library *l, const char *rel, uint32_t parent_idx,
                    uint32_t *out_idx) {
  if (l->dir_len == l->dir_cap) {

    size_t new_cap;

    if (l->dir_cap == 0) {
      new_cap = 64;
    } else {
      new_cap = l->dir_cap * 2;
    }

    struct dir *new_dirs = realloc(l->dirs, new_cap * sizeof(*l->dirs));

    if (!new_dirs) {
      LOG_ERR(TAG, "realloc FAILED for directories");
      return -1;
    }

    l->dirs = new_dirs;
    l->dir_cap = new_cap;
  }

  uint32_t new_idx =
      (uint32_t)l->dir_len; // new index that the child dir now is registered to
  const char *name = strrchr(rel, '/');

  if (name) {
    name++;
  } else if (rel[0] != '\0') {
    name = rel;
  } else {
    name = "pssst-player";
  }

  char *name_copy = strdup(name);

  if (!name_copy) {
    LOG_ERR(TAG, "strdup FAILED for directory");
    return -1;
  }
  l->dirs[new_idx] = (struct dir){
      .id = fnv1a32(rel, strlen(rel)),
      .name = name_copy,
      .parent = parent_idx,
  };

  l->dir_len++;

  if (parent_idx != ROOT_DIR) {

    if (parent_idx >= l->dir_len) {
      LOG_ERR(TAG, "invalid parent directory index %u", parent_idx);
      free(name_copy);
      l->dir_len--;
      return -1;
    }

    if (dir_add_child_dir(&l->dirs[parent_idx], new_idx) < 0) {
      free(name_copy);
      memset(&l->dirs[new_idx], 0, sizeof(l->dirs[new_idx]));
      l->dir_len--;
      return -1;
    }
  }

  *out_idx = new_idx;

  return 0;
}

static int library_push(struct library *l, const char *root, const char *rel,
                        uint32_t parent_idx) {
  char full[MAX_PATH];

  if (join_path(full, sizeof(full), root, rel) < 0) {
    return -1;
  }
  if (l->len == l->cap) {
    size_t new_cap;
    if (l->cap == 0) {
      new_cap = 64;
    } else {
      new_cap = l->cap * 2;
    }
    struct item *new_items = realloc(l->items, new_cap * sizeof(*l->items));

    if (!new_items) {
      LOG_ERR(TAG, "realloc FAILED OOM");
      return -1;
    }

    l->items = new_items;
    l->cap = new_cap;
  }

  char *path_copy = strdup(full);

  if (!path_copy) {
    LOG_ERR(TAG, "strdup FAILED for item path");
    return -1;
  }
  uint32_t new_idx = (uint32_t)l->len;

  const char *name = strrchr(rel, '/');

  if (name) {
    name++;
  } else {
    name = rel;
  }

  char *name_copy = strdup(name);

  if (!name_copy) {
    free(path_copy);
    LOG_ERR(TAG, "strdup FAILED for item name");
    return -1;
  }

  l->items[new_idx] = (struct item){
      .id = fnv1a32(rel, strlen(rel)),
      .path = path_copy,
      .name = name_copy,
      .parent = parent_idx,
  };

  l->len++;

  if (dir_add_child_item(&l->dirs[parent_idx], new_idx) < 0) {
    free(path_copy);
    free(name_copy);

    memset(&l->items[new_idx], 0, sizeof(l->items[new_idx]));

    l->len--;

    return -1;
  }

  return 0;
}

/**
 * @brief Recursively scan one directory subtree
 *
 * @param l Output library
 * @param root Media root directory
 * @param rel Relative path within root
 * @param parent_idx used to keep track of parent dirs in recursive calls of
 * itself
 *
 * @return 0=Success, -1=Failure
 */
static int scan_dir(struct library *l, const char *root, const char *rel,
                    uint32_t parent_idx) {
  char full[MAX_PATH];
  // 32 bit system lmeow

  DIR *d;
  struct dirent *e;

  if (rel[0] == '\0') {
    if (snprintf(full, MAX_PATH, "%s", root) >= (int)sizeof(full))
      return -1;
  } else {
    if (join_path(full, MAX_PATH, root, rel) < 0)
      return -1;
  }

  d = opendir(full);
  if (!d) {
    LOG_ERR(TAG, "opendir FAILED     %s", full);
    return -1;
  }
  uint32_t my_idx;
if (dir_push(l, rel, parent_idx, &my_idx) < 0) {
  closedir(d);
  return -1;
}
  while ((e = readdir(d)) != NULL) {
    if (strcmp(e->d_name, ".") == 0)
      continue;
    if (strcmp(e->d_name, "..") == 0)
      continue;

    char rel2[MAX_PATH];
    char full2[MAX_PATH];

    if (rel[0] == '\0') {
      if (snprintf(rel2, sizeof(rel2), "%s", e->d_name) >= (int)sizeof(rel2))
        goto fail;
    } else {
      if (join_path(rel2, sizeof(rel2), rel, e->d_name) < 0)
        goto fail;
    }

    if (join_path(full2, sizeof(full2), root, rel2) < 0)
      goto fail;

    struct stat st;
    if (lstat(full2, &st) < 0)
      continue;

    if (S_ISDIR(st.st_mode)) {
      // call myself and pass in a parent_idx basically telling myself
      // afterwards that this x index is your parent
      if (scan_dir(l, root, rel2, my_idx) < 0)
        goto fail;
      continue;
    }

    if (S_ISREG(st.st_mode)) {
      if (!is_supported_media(rel2))
        continue;

      if (library_push(l, root, rel2, my_idx) < 0)
        goto fail;
      continue;
    }
  }

  closedir(d);
  return 0;

fail:
  closedir(d);
  return -1;
}

int scan_library(struct library *l, const char *root) {
  memset(l, 0, sizeof(*l));
  return scan_dir(l, root, "", ROOT_DIR);
}

void scan_library_free(struct library *l) {
  if (!l)
    return;

  for (size_t i = 0; i < l->len; i++) {
    free(l->items[i].path);
    free(l->items[i].name);
  }

  free(l->items);

  l->items = NULL;

  for (size_t i = 0; i < l->dir_len; i++) {
    free(l->dirs[i].name);
    free(l->dirs[i].child_dirs);
    free(l->dirs[i].child_items);
  }

  free(l->dirs);

  l->len = 0;
  l->cap = 0;
  memset(l, 0, sizeof(*l));
}

int scan_library_rescan(struct library *l, const char *root) {
  struct library new;
  struct library old;

  if (!l || !root)
    return -1;

  memset(&new, 0, sizeof(new));

  if (scan_library(&new, root) < 0) {
    scan_library_free(&new);
    return -1;
  }

  /* swap */
  old = *l;
  *l = new;

  /* free old */
  scan_library_free(&old);

  return 0;
}
