//////////////////////////////////////////////////////////////////////

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <esp_err.h>

//////////////////////////////////////////////////////////////////////

typedef struct _font_graphic
{
    int8_t offset_x;
    int8_t offset_y;
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
} font_graphic;

//////////////////////////////////////////////////////////////////////

typedef struct font_data
{
    font_graphic *graphics;
    int16_t *lookup;
    uint8_t *advances;
    int height;
    int num_graphics;
    int num_lookup;
    int num_advances;
} font_data;

struct font_t;

typedef struct font_t *font_handle_t;

//////////////////////////////////////////////////////////////////////

esp_err_t font_init(font_data const *fnt, char const *name, uint8_t const *png_start, uint8_t const *png_end, font_handle_t *handle);

void font_drawtext(font_handle_t fnt, uint16_t *lcd_buffer, vec2i const *pos, uint8_t const *text, uint16_t back_color);

void font_measure_string(font_handle_t fnt, uint8_t const *text, vec2i *size);

//////////////////////////////////////////////////////////////////////

#define __FONT_JOIN2(x, y) x##y
#define __FONT_JOIN(x, y) __FONT_JOIN2(x, y)

#define FONT_INIT(fontname, handle) \
    font_init(&__FONT_JOIN(fontname, _font), #fontname, __FONT_JOIN(fontname, _png_start), __FONT_JOIN(fontname, _png_end), handle)

// E.G. esp_err_t ret = FONT_INIT(Cascadia, &my_font_handle)

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif
