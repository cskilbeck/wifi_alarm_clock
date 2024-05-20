//////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <esp_err.h>
#include "util.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct image
{
    uint32_t const *pixel_data;
    int width;
    int height;
    int image_id;
} image_t;

//////////////////////////////////////////////////////////////////////

esp_err_t image_init();

image_t const *image_get(int image_id);
image_t const *image_get_unchecked(int image_id);

esp_err_t image_decode_png(char const *name, int *out_image_id, uint8_t const *png_data, size_t png_size);

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif
