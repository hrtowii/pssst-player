#include "config.h"
#include "logging.h"
#include "util.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TAG "config"
static void set_defaults(struct config *cfg) {
  strncpy(cfg->media_dir, "ms0:/PSP/MUSIC/", MAX_PATH - 1);
  cfg->media_dir[MAX_PATH - 1] = '\0';
  cfg->volume = 80;
  cfg->shuffle = 0;
}

static char *trim(char *s) {
  while (*s == ' ' || *s == '\t')
    s++;
  char *end = s + strlen(s) - 1;
  while (end > s &&
         (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
    *end = '\0';
    end--;
  }
  return s;
}

// config is a stack struct inited from main, no malloc or frees needed cus its
// small enough

int config_load(const char *path, struct config *cfg) {
  // things to do:
  // 1. read the curr path. if nothing, init defaults, then populate the struct,
  // writeback to file, ret 0
  set_defaults(cfg);
  FILE *f = fopen(path, "r");
  if (!f) {
    LOG_INFO(TAG, "either first boot or file not found, writing back defaults");
    char config_dir[MAX_PATH];
    strncpy(config_dir, path, MAX_PATH - 1);
    config_dir[MAX_PATH - 1] = '\0';
    char *slash = strrchr(config_dir, '/');
    if (slash) *slash = '\0';


    if (mkdir_p(config_dir) < 0) {
      LOG_INFO(TAG, "failed to create config dir");
      return -1;
    }
    FILE *out = fopen(path, "w");
    if (!out) {
      LOG_INFO(TAG, "failed to create config file at %s", path);
      return -1;
    }

    const char *defaults = "media_dir=ms0:/PSP/MUSIC/\n"
                           "volume=80\n"
                           "shuffle=1\n";
    size_t written = fwrite(defaults, sizeof(char), strlen(defaults), out);
    fclose(out);
    if (written != strlen(defaults)) {
      LOG_INFO(TAG, "short write on config defaults");
      return -1;
    }
    return 0;
  }
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    char *l = trim(line);
    if (l[0] == '\0' || l[0] == '#')
      continue;
    char *eq = strchr(l, '=');
    if (!eq)
      continue;
    *eq = '\0';
    char *key = trim(l);
    char *val = trim(eq + 1);
    if (strcmp(key, "media_dir") == 0) {
      strncpy(cfg->media_dir, val, MAX_PATH - 1);
      cfg->media_dir[MAX_PATH - 1] = '\0';
    } else if (strcmp(key, "volume") == 0) {
      cfg->volume = atoi(val);
    } else if (strcmp(key, "shuffle") == 0) {
      cfg->shuffle = atoi(val);
    }
  }
  fclose(f);
  return 0;
}
