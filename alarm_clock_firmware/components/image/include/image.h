//////////////////////////////////////////////////////////////////////

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <esp_err.h>
#include "util.h"

typedef struct image
{
    uint32_t *pixel_data;
    int width;
    int height;
} image_t;

//////////////////////////////////////////////////////////////////////

image_t const *image_get(int image_id);

esp_err_t image_decode_png(int *out_image_id, uint8_t const *png_data, size_t png_size);

// void image_blit(int source_image_id, uint16_t *lcd_buffer, vec2i const *src_pos, vec2i const *dst_pos, vec2i const *size);

// void image_blit_noclip(int source_image_id, uint16_t *lcd_buffer, vec2i const *src_pos, vec2i const *dst_pos, vec2i const *size);

// void image_fillrect(uint16_t *lcd_buffer, vec2i const *topleft, vec2i const *size, uint16_t pixel);

// void image_fillrect_noclip(uint16_t *lcd_buffer, vec2i const *topleft, vec2i const *size, uint16_t pixel);

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif
