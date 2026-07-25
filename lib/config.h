#include "util.h"
#include <stdbool.h>
struct config {
	char wallpaper[MAX_PATH];
	char media_dir[MAX_PATH];
	int volume;
	bool shuffle;
};

// example config:
// media_dir=ms0:/PSP/MUSIC/
// volume=80
// shuffle=0

int config_load(const char *path, struct config *cfg);
// config is a stack struct inited from main, no malloc or frees needed cus its small enough
