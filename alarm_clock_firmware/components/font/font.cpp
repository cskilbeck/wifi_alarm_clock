//////////////////////////////////////////////////////////////////////

#include <freertos/FreeRTOS.h>

#include <esp_log.h>
#include <esp_heap_caps.h>

#include "util.h"
#include "image.h"
#include "font.h"

static char const *TAG = "font";

//////////////////////////////////////////////////////////////////////

typedef struct font_t
{
    char const *name;
    int image_index;
    font_data const *font_struct;
} font_t;

//////////////////////////////////////////////////////////////////////

void font_drawtext(font_handle_t fnt, uint16_t *lcd_buffer, vec2 const *pos, uint8_t const *text, uint16_t back_color)
{
    assert(fnt != nullptr);
    assert(fnt->image_index != 0);
    assert(lcd_buffer != nullptr);
    assert(pos != nullptr);
    assert(text != nullptr);

    image_t const *img = image_get(fnt->image_index);

    font_data const *f = fnt->font_struct;

    int char_height = f->height;

    vec2 curpos = *pos;

    for(uint8_t c = *text; c != 0; c = *++text) {

        int char_width = f->advances[c];

        if(char_width <= 0) {
            continue;
        }

        int glyph_index = f->lookup[c];

        if(glyph_index < 0) {
            vec2 fillsize = { char_width, char_height };
            image_fillrect(lcd_buffer, &curpos, &fillsize, back_color);
        } else {
            font_graphic const &graphic = f->graphics[glyph_index];
            vec2 src = { graphic.x, graphic.y };
            vec2 offset = { graphic.offset_x, graphic.offset_y };
            vec2 dst = { curpos.x + offset.x, curpos.y + offset.y };
            vec2 size = { graphic.width, graphic.height };

            // top
            if(offset.y != 0) {
                vec2 fillsize = { char_width, offset.y };
                image_fillrect(lcd_buffer, &curpos, &fillsize, back_color);
            }

            // left
            if(offset.x != 0) {
                vec2 fillpos = { curpos.x, curpos.y + offset.y };
                vec2 fillsize = { offset.x, char_height - offset.y };
                image_fillrect(lcd_buffer, &fillpos, &fillsize, back_color);
            }

            // right
            int rightremain = char_width - (offset.x + size.x);
            if(rightremain > 0) {
                vec2 fillpos = { dst.x + size.x, curpos.y + offset.y };
                vec2 fillsize = { rightremain, char_height - offset.y };
                image_fillrect(lcd_buffer, &fillpos, &fillsize, back_color);
            }

            // bottom strip
            int bottomremain = char_height - (offset.y + size.y);
            if(bottomremain > 0) {
                vec2 fillpos = { dst.x, dst.y + size.y };
                vec2 fillsize = { size.x, bottomremain };
                image_fillrect(lcd_buffer, &fillpos, &fillsize, back_color);
            }

            image_blit(fnt->image_index, lcd_buffer, &src, &dst, &size);
        }
        curpos.x += char_width;
    }
}

//////////////////////////////////////////////////////////////////////

void font_measure_string(font_handle_t fnt, uint8_t const *text, vec2 *size)
{
    assert(size != nullptr);
    assert(text != nullptr);
    assert(fnt != nullptr);

    font_data const *f = fnt->font_struct;

    int width = 0;
    for(uint8_t c = *text; c != 0; c = *++text) {
        width += f->advances[c];
    }
    size->x = width;
    size->y = f->height;
}

//////////////////////////////////////////////////////////////////////

esp_err_t font_init(font_data const *fnt, char const *name, uint8_t const *png_start, uint8_t const *png_end, font_handle_t *handle)
{
    ESP_LOGI(TAG, "init font %s", name);

    if(handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    font_handle_t new_font = (font_handle_t)calloc(1, sizeof(font_t));

    if(new_font == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    new_font->name = name;
    new_font->font_struct = fnt;
    esp_err_t ret = image_decode_png(&new_font->image_index, png_start, png_end - png_start);

    if(ret == ESP_OK) {
        *handle = new_font;
    }
    return ret;
}
