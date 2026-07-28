#define STB_IMAGE_IMPLEMENTATION
#include "display/stb_image.h"

#include "ui.h"
#include "util.h"
#include <malloc.h>
#include <pspkernel.h>
#include "logging.h"
#define TAG	"img_loader"

PSPTexture load_texture(const char *path)
{
    PSPTexture texture = {0};
    int channels = 0;
    unsigned char *image = stbi_load(
        path,
        &texture.width,
        &texture.height,
        &channels,
        4
    );
    if (image == NULL) {
        LOG_DEBUG(TAG,
            "stbi failed loading %s: %s\n",
            path,
            stbi_failure_reason()
        );
        return texture;
    }

    texture.textureWidth = next_pow2_32(texture.width);
    texture.textureHeight = next_pow2_32(texture.height);

    if (texture.textureWidth > 512 || texture.textureHeight > 512) {
        LOG_DEBUG(TAG,
            "%s is %dx%d (rounds to %dx%d) > 512x512 limit, refusing to load\n",
            path, texture.width, texture.height,
            texture.textureWidth, texture.textureHeight
        );
        stbi_image_free(image);
        return (PSPTexture){0};
    }
// checked it's already <= 512 so you can copy the data from stb in without padding it
    texture.pixels = memalign(16, texture.width * texture.height * 4);
    if (texture.pixels == NULL) {
        LOG_DEBUG(TAG, "texture alloc failed\n");
        stbi_image_free(image);
        return (PSPTexture){0};
    }

    memcpy(texture.pixels, image, texture.width * texture.height * 4);
    stbi_image_free(image);

    sceKernelDcacheWritebackRange(
        texture.pixels,
        texture.width * texture.height * 4
    );

    LOG_DEBUG(TAG,
        "loaded %s: %dx%d, GE dims %dx%d\n",
        path,
        texture.width, texture.height,
        texture.textureWidth, texture.textureHeight
    );

    return texture;
}
