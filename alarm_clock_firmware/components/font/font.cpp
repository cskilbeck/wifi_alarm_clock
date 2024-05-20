//////////////////////////////////////////////////////////////////////

#include <freertos/FreeRTOS.h>

#include <esp_log.h>
#include <esp_heap_caps.h>

#include "util.h"
#include "image.h"
#include "font.h"
#include "display.h"

LOG_CONTEXT("font");

//////////////////////////////////////////////////////////////////////

esp_err_t font_drawtext(font_handle_t fnt, vec2i const *pos, uint8_t const *text, uint8_t alpha, int blend_mode)
{
    if(fnt == nullptr || pos == nullptr || text == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if(fnt->image_index == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    font_data const *f = fnt->font_struct;

    if(f == nullptr || f->graphics == nullptr || f->num_graphics == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    vec2i curpos = *pos;

    for(uint8_t c = *text; c != 0; c = *++text) {

        int char_width = f->advances[c];

        if(char_width <= 0) {
            continue;
        }

        int glyph_index = f->lookup[c];

        if(glyph_index >= 0) {
            font_graphic const &graphic = f->graphics[glyph_index];
            vec2i src = { graphic.x, graphic.y };
            vec2i offset = { graphic.offset_x, graphic.offset_y };
            vec2i dst = { curpos.x + offset.x, curpos.y + offset.y };
            vec2i size = { graphic.width, graphic.height };

            display_imagerect(&dst, &src, &size, fnt->image_index, alpha, blend_mode);
        }
        curpos.x += char_width;
    }
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t font_measure_string(font_handle_t fnt, uint8_t const *text, vec2i *size)
{
    if(size == nullptr || text == nullptr || fnt == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    font_data const *f = fnt->font_struct;

    int width = 0;
    for(uint8_t c = *text; c != 0; c = *++text) {
        width += f->advances[c];
    }
    size->x = width;
    size->y = f->height;

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t font_init(font_data const *fnt, char const *name, uint8_t const *png_start, uint8_t const *png_end, font_handle_t *handle)
{
    LOG_D("init font %s", name);

    if(handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    font_handle_t new_font = (font_handle_t)calloc(1, sizeof(font_t));

    if(new_font == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    new_font->name = name;
    new_font->font_struct = fnt;
    esp_err_t ret = image_decode_png(name, &new_font->image_index, png_start, png_end - png_start);

    if(ret == ESP_OK) {
        *handle = new_font;
    }
    return ret;
}
