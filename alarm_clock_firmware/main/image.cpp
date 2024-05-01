#include <freertos/FreeRTOS.h>

#include <esp_mem.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

#include "pngle.h"
#include "image.h"
#include "lcd.h"

static char const *TAG = "image";

namespace
{
    template <typename T> T min(T const &a, T const &b)
    {
        return a < b ? a : b;
    }

    template <typename T> T max(T const &a, T const &b)
    {
        return a > b ? a : b;
    }

    void setpixel(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
    {
        image_t *img = (image_t *)pngle_get_user_data(pngle);
        uint32_t r = (rgba[0] >> 3) & 0x1f;
        uint32_t g = (rgba[1] >> 2) & 0x3f;
        uint32_t b = (rgba[2] >> 3) & 0x1f;
        r <<= 11;
        g <<= 5;
        b <<= 0;
        // uint16_t pixel = (uint16_t)0b0000011111000000;
        // uint16_t pixel = (uint16_t)(g);
        uint16_t pixel = (uint16_t)(r | g | b);
        pixel = (pixel >> 8) | (pixel << 8);
        img->pixel_data[x + y * img->width] = pixel;
    }

    void on_init(pngle_t *pngle, uint32_t w, uint32_t h)
    {
        image_t *img = (image_t *)pngle_get_user_data(pngle);

        ESP_LOGI(TAG, "PNGLE INIT: %ld, %ld", w, h);
        img->width = w;
        img->height = h;
        img->pixel_data = (uint16_t *)heap_caps_malloc(w * h * 2, MALLOC_CAP_SPIRAM);
        assert(img->pixel_data != NULL);
    }

}    // namespace

void image_decode_png(image_t *image, uint8_t const *png_data, size_t png_size)
{
    pngle_t *pngle = pngle_new();
    pngle_set_user_data(pngle, image);
    pngle_set_init_callback(pngle, on_init);
    pngle_set_draw_callback(pngle, setpixel);
    int err = pngle_feed(pngle, png_data, png_size);
    if(err < 0) {
        ESP_LOGE(TAG, "PNGLE Error %d", err);
    }
    pngle_destroy(pngle);
}

// lcd_buffer is fixed at LCD_WIDTH, LCD_HEIGHT !

void image_blit_noclip(image_t const *source_image, uint16_t *lcd_buffer, vec2_t const *src_pos, vec2_t const *dst_pos, vec2_t const *size)
{
    uint16_t *dst = lcd_buffer + dst_pos->x + dst_pos->y * LCD_WIDTH;
    uint16_t *src = source_image->pixel_data + src_pos->x + src_pos->y * source_image->width;
    for(int y = 0; y < size->y; ++y) {
        uint16_t *dst_row = dst;
        uint16_t *src_row = src;
        for(int x = 0; x < size->x; ++x) {
            *dst_row++ = *src_row++;
        }
        src += source_image->width;
        dst += LCD_WIDTH;
    }
}

void image_blit(image_t const *source_image, uint16_t *lcd_buffer, vec2_t const *src_pos, vec2_t const *dst_pos, vec2_t const *size)
{
    vec2_t clipped_src_pos = vec2_t{ min(src_pos->x, source_image->width - 1), min((int)src_pos->y, source_image->height - 1) };

    vec2_t clipped_dst_pos = vec2_t{ min((int)dst_pos->x, LCD_WIDTH - 1), min((int)dst_pos->y, LCD_HEIGHT - 1) };

    vec2_t clipped_size = vec2_t{ min(min(size->x, source_image->width - src_pos->x), LCD_WIDTH - dst_pos->x),
                                  min(min(size->y, source_image->height - src_pos->y), LCD_HEIGHT - dst_pos->y) };

    if(clipped_size.x > 0 && clipped_size.y > 0) {
        image_blit_noclip(source_image, lcd_buffer, &clipped_src_pos, &clipped_dst_pos, &clipped_size);
    }
}

void image_fillrect_noclip(uint16_t *lcd_buffer, vec2_t const *topleft, vec2_t const *size, uint16_t pixel)
{
    pixel = (pixel >> 8) | (pixel << 8);
    uint16_t *dst = lcd_buffer + topleft->x + topleft->y * LCD_WIDTH;
    for(int y = 0; y < size->y; ++y) {
        uint16_t *dst_row = dst;
        for(int x = 0; x < size->x; ++x) {
            *dst_row++ = pixel;
        }
        dst += LCD_WIDTH;
    }
}

void image_fillrect(uint16_t *lcd_buffer, vec2_t const *topleft, vec2_t const *size, uint16_t pixel)
{
    vec2_t clipped_topleft = vec2_t{ min(topleft->x, LCD_WIDTH - 1), min(topleft->y, LCD_HEIGHT - 1) };
    vec2_t clipped_size = vec2_t{ min(size->x, LCD_WIDTH - (clipped_topleft.x + size->x)), min(size->y, LCD_HEIGHT - -(clipped_topleft.y + size->y)) };
    if(clipped_size.x > 0 && clipped_size.y > 0) {
        image_fillrect_noclip(lcd_buffer, &clipped_topleft, &clipped_size, pixel);
    }
}
