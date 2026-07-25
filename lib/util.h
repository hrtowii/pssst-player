#include <stddef.h>
#include <stdio.h>
#define MAX_PATH	260

// given argv, return the current working directory of the app. helper to js add in app:/config.txt, app:/wallpaper.png etc. for config to read from
int get_app_dir(int argc, char *argv[], char *out_dir, size_t out_size);

int join_path(char* out, size_t outsz, const char* a, const char* b);

void die(const char* s);

