#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <esp_err.h>

typedef struct vec2_t
{
    int x;
    int y;
} vec2_t;

typedef struct image
{
    uint16_t *pixel_data;
    int width;
    int height;
} image_t;

void image_decode_png(image_t *image, uint8_t const *png_data, size_t png_size);

void image_blit_noclip(image_t const *source_image, uint16_t *lcd_buffer, vec2_t const *src_pos, vec2_t const *dst_pos, vec2_t const *size);

void image_blit(image_t const *source_image, uint16_t *lcd_buffer, vec2_t const *src_pos, vec2_t const *dst_pos, vec2_t const *size);

void image_fillrect(uint16_t *lcd_buffer, vec2_t const *topleft, vec2_t const *size, uint16_t pixel);

#if defined(__cplusplus)
}
#endif
