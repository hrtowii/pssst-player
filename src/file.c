#include "file.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include "logging.h"
#include "util.h"

#define TAG	"file_scanner"
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

static int library_push(struct library* l, const char* root, const char* rel)
{
	char full[MAX_PATH];
	if (join_path(full, sizeof(full), root, rel) < 0)
        	return -1;
	if (l->len == l->cap) {
		size_t newcap = l->cap ? (l->cap * 2) : 64;
		struct item* newitems = realloc(l->items, newcap * sizeof(*newitems));
		if (!newitems) {
			LOG_ERR(TAG, "realloc FAILED OOM");
			return -1;
		}
		l->items = newitems;
		l->cap = newcap;
	}

	char* p = strdup(full);
	if (!p)
		return -1;

	l->items[l->len].id = fnv1a32(rel, strlen(rel));
	l->items[l->len].path = p;
	l->len++;

	return 0;
}

/**
 * @brief Recursively scan one directory subtree
 *
 * @param l Output library
 * @param root Media root directory
 * @param rel Relative path within root
 *
 * @return 0=Success, -1=Failure
 */
static int scan_dir(struct library* l, const char* root, const char* rel)
{
	char full[MAX_PATH];
	// 32 bit system lmeow

	DIR* d;
	struct dirent* e;

	if (rel[0] == '\0') {
		if (snprintf(full, MAX_PATH, "%s", root) >= (int)sizeof(full))
			return -1;
	}
	else {
		if (join_path(full, MAX_PATH, root, rel) < 0)
			return -1;
	}

	d = opendir(full);
	if (!d) {
		LOG_ERR(TAG, "opendir FAILED     %s", full);
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
		}
		else {
			if (join_path(rel2, sizeof(rel2), rel, e->d_name) < 0)
				goto fail;
		}

		if (join_path(full2, sizeof(full2), root, rel2) < 0)
			goto fail;

		struct stat st;
		if (lstat(full2, &st) < 0)
			continue;

		if (S_ISDIR(st.st_mode)) {
			if (scan_dir(l, root, rel2) < 0)
				goto fail;
			continue;
		}

		if (S_ISREG(st.st_mode)) {
			if (library_push(l, root, rel2) < 0)
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

int scan_library(struct library* l, const char* root)
{
	memset(l, 0, sizeof(*l));
	return scan_dir(l, root, "");
}

void scan_library_free(struct library* l)
{
	if (!l)
		return;

	for (size_t i = 0; i < l->len; i++)
		free(l->items[i].path);

	free(l->items);

	l->items = NULL;
	l->len = 0;
	l->cap = 0;
}

int scan_library_rescan(struct library* l, const char* root)
{
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
