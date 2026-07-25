#include "util.h"
#include <string.h>
#include <stdio.h>
#include "psploadexec.h"
#include "util.h"
#include <string.h>

int get_app_dir(int argc, char *argv[], char *out_dir, size_t out_size)
{
    if (argc < 1 || argv == NULL || argv[0] == NULL) {
        strncpy(out_dir, "ms0:/PSP/GAME/pssst_player/", out_size - 1);
        out_dir[out_size - 1] = '\0';
        return -1;
    }

    strncpy(out_dir, argv[0], out_size - 1);
    out_dir[out_size - 1] = '\0';

    char *slash = strrchr(out_dir, '/');
    if (slash != NULL) {
        *(slash + 1) = '\0';
    } else {
        strncpy(out_dir, "ms0:/PSP/GAME/pssst_player/", out_size - 1);
        out_dir[out_size - 1] = '\0';
        return -1;
    }

    return 0;
}

int join_path(char* out, size_t outsz, const char* a, const char* b)
{
	if (!out || outsz == 0 || !a || !b)
		return -1;

	if (out == a || out == b)
		return -1;

	if (a[0] == '\0') {
		size_t bl = strlen(b);
		if (bl >= outsz) 
			return -1;
		memcpy(out, b, bl + 1);
		return 0;
	}

	if (b[0] == '\0') {
		size_t al = strlen(a);
		if (al >= outsz) 
			return -1;
		memcpy(out, a, al + 1);
		return 0;
	}

	size_t al = strlen(a);
	size_t bl = strlen(b);

	int needs_slash = (a[al - 1] != '/') ? 1 : 0;
	size_t total_len = al + needs_slash + bl;

	if (total_len >= outsz)
		return -1;

	memcpy(out, a, al);
	
	if (needs_slash) {
		out[al] = '/';
	}
	
	memcpy(out + al + needs_slash, b, bl);
	out[total_len] = '\0'; // Manual null-termination

	return 0;
}

void die(const char *s)
{
	perror(s);
	sceKernelExitGame();
}
