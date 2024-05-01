#include <freertos/FreeRTOS.h>

#include <esp_mem.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

#include "image.h"
#include "font.h"

static char const *TAG = "font";

void font_drawtext(font *fnt, image_t *src_image, uint16_t *lcd_buffer, vec2_t const *pos, uint8_t const *text)
{
    // uint16_t back_color = 0b1111100000000000;
    uint16_t back_color = 0x0000;

    // fill in the edges if necessary
    int char_height = fnt->height;

    vec2_t curpos = *pos;

    for(uint8_t c = *text; c != 0; c = *++text) {

        int char_width = fnt->advances[c];

        if(char_width <= 0) {
            continue;
        }

        int glyph_index = fnt->lookup[c];

        ESP_LOGI(TAG, "%c: index %d", c, glyph_index);

        if(glyph_index < 0) {
            vec2_t fillsize = { char_width, char_height };
            image_fillrect(lcd_buffer, &curpos, &fillsize, back_color);
        } else {
            font_graphic const &graphic = fnt->graphics[glyph_index];
            vec2_t src = { graphic.x, graphic.y };
            vec2_t offset = { graphic.offset_x, graphic.offset_y };
            vec2_t dst = { curpos.x + offset.x, curpos.y + offset.y };
            vec2_t size = { graphic.width, graphic.height };

            ESP_LOGI(TAG, "%c %d,%d (%dx%d)", c, src.x, src.y, size.x, size.y);

            // top
            if(offset.y != 0) {
                vec2_t fillsize = { char_width, offset.y };
                image_fillrect(lcd_buffer, &curpos, &fillsize, back_color);
            }

            // left
            if(offset.x != 0) {
                vec2_t fillpos = { curpos.x, curpos.y + offset.y };
                vec2_t fillsize = { offset.x, char_height - offset.y };
                image_fillrect(lcd_buffer, &fillpos, &fillsize, back_color);
            }

            // right
            int rightremain = char_width - (offset.x + size.x);
            if(rightremain > 0) {
                vec2_t fillpos = { dst.x + size.x, curpos.y + offset.y };
                vec2_t fillsize = { rightremain, char_height - offset.y };
                image_fillrect(lcd_buffer, &fillpos, &fillsize, back_color);
            }

            // bottom strip
            int bottomremain = char_height - (offset.y + size.y);
            if(bottomremain > 0) {
                vec2_t fillpos = { dst.x, dst.y + size.y };
                vec2_t fillsize = { size.x, bottomremain };
                image_fillrect(lcd_buffer, &fillpos, &fillsize, back_color);
            }

            image_blit(src_image, lcd_buffer, &src, &dst, &size);
        }
        curpos.x += char_width;
    }
}