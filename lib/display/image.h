#include "display/graphics.h"
#include <stddef.h>

PSPTexture load_texture(const char *path);
PSPTexture create_texture_from_rgba(const unsigned char *rgba, int w, int h);
PSPTexture album_art_from_data(const unsigned char *data, size_t len);
