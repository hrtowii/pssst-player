#include <stdint.h>
#include <stddef.h>
#define ROOT_DIR UINT32_MAX

// copied from parados for recursive scanning
// https://github.com/uint23/parados/blob/master/server/include/scan.h
struct item {
	uint32_t       id;
	char*          path;
	char*          name;
	uint32_t       parent;
};

// i want a recursive file tree structure that can collapse and show stuff, because it gets annoying to navigate

struct dir {
	uint32_t       id;
	char*          name;
	uint32_t       parent;

	uint32_t*      child_dirs;  // indices into library->dirs
	size_t         child_dir_len, child_dir_cap;

	uint32_t*      child_items; // indices into library->items
	size_t         child_item_len, child_item_cap;
};

// basically. the library now contains a list of items and directories. libraries->dirs[0..x] contains indexes of what are items (Songs)
// so say that l->dirs[2].child_dirs[0] == 3 
// l->dirs[3] is the child of l->dirs[2]

struct library {
	struct item*   items;
	size_t         len;
	size_t         cap;

	struct dir*    dirs;
	size_t         dir_len, dir_cap;
};



/**
 * @brief Free all memory owned by a library
 *
 * @note Safe to call multiple times
 *
 * @param l Library to free
 */
void scan_library_free(struct library* l);

/**
 * @brief Build library by recursively scanning given directory
 *
 * @param l Output library (initialised by function)
 * @param root Media root directory
 *
 * @return 0=Success, -1=Failure
 */
int scan_library(struct library* l, const char* root);

/**
 * @brief Rescan given directory and replace current library contents
 *
 * @note On success, old library contents are freed.
 *
 * @param l Library to replace
 * @param root Media root directory
 *
 * @return 0=Success, -1=Failure
 */
int scan_library_rescan(struct library* l, const char* root);

