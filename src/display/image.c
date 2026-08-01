#define STB_IMAGE_IMPLEMENTATION
#include "display/stb_image.h"
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "display/stb_image_resize2.h"

#include "display/image.h"
#include "display/ui.h"
#include "util/util.h"
#include <malloc.h>
#include <pspkernel.h>
#include "util/logging.h"
#define TAG	"img_loader"

static PSPTexture upload_rgba(const unsigned char *rgba, int w, int h)
{
    PSPTexture texture = {0};
    texture.width = w;
    texture.height = h;
    texture.textureWidth = next_pow2_32(w);
    texture.textureHeight = next_pow2_32(h);

    if (texture.textureWidth > 512 || texture.textureHeight > 512)
        return texture;
// dont need to pad js memcpy in
    texture.pixels = memalign(16, w * h * 4);
    if (texture.pixels == NULL)
        return texture;

    memcpy(texture.pixels, rgba, w * h * 4);

    sceKernelDcacheWritebackRange(texture.pixels, w * h * 4);

    return texture;
}

PSPTexture load_texture(const char *path)
{
    int w, h, channels;
    unsigned char *image = stbi_load(path, &w, &h, &channels, 4);
    if (image == NULL) {
        LOG_ERR(TAG, "stbi failed loading %s: %s\n", path, stbi_failure_reason());
        return (PSPTexture){0};
    }

    PSPTexture texture = upload_rgba(image, w, h);
    stbi_image_free(image);

    if (texture.pixels == NULL)
        LOG_ERR(TAG, "upload failed for %s (%dx%d)\n", path, w, h);
    else
        LOG_DEBUG(TAG, "loaded %s: %dx%d, GE dims %dx%d\n", path, w, h,
                  texture.textureWidth, texture.textureHeight);

    return texture;
}

PSPTexture create_texture_from_rgba(const unsigned char *rgba, int w, int h)
{
    return upload_rgba(rgba, w, h);
}

PSPTexture album_art_from_data(const unsigned char *data, size_t len)
{
    if (!data || len == 0)
        return (PSPTexture){0};

    int w, h, ch;
    unsigned char *decoded = stbi_load_from_memory(
        data, (int)len, &w, &h, &ch, 4);
    if (!decoded)
        return (PSPTexture){0};

    int mw = w > 100 ? 100 : w;
    int mh = h > 100 ? 100 : h;
    PSPTexture tex = {0};
    if (mw > 0 && mh > 0) {
        unsigned char *resized = malloc(mw * mh * 4);
        if (resized) {
            stbir_resize_uint8_linear(decoded, w, h, 0, resized, mw, mh, 0, STBIR_RGBA);
            tex = create_texture_from_rgba(resized, mw, mh);
            free(resized);
        }
    }
    stbi_image_free(decoded);
    return tex;
}
