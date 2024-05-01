#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <esp_err.h>

typedef struct _font_graphic
{
    int8_t offset_x;
    int8_t offset_y;
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
} font_graphic;

typedef struct _font
{
    font_graphic *graphics;
    int16_t *lookup;
    uint8_t *advances;
    int height;
    int num_graphics;
    int num_lookup;
    int num_advances;
} font;

void font_drawtext(font *fnt, image_t *src_image, uint16_t *lcd_buffer, vec2_t const *pos, uint8_t const *text);

#if defined(__cplusplus)
}
#endif
