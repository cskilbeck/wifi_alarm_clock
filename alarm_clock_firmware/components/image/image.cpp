//////////////////////////////////////////////////////////////////////

#include <freertos/FreeRTOS.h>

#include <esp_log.h>
#include <esp_heap_caps.h>

#include "pngle.h"
#include "util.h"
#include "image.h"
#include "util.h"
#include "lcd_gc9a01.h"

static char const *TAG = "image";

//////////////////////////////////////////////////////////////////////

namespace
{
    int constexpr MAX_IMAGES = 128;

    image_t images[MAX_IMAGES];
    int num_images = 0;

    //////////////////////////////////////////////////////////////////////

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

    //////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////

image_t const *image_get(int image_id)
{
    if(image_id <= 0 || image_id > num_images) {
        return NULL;
    }
    return images + image_id;
}

//////////////////////////////////////////////////////////////////////

esp_err_t image_decode_png(int *out_image_id, uint8_t const *png_data, size_t png_size)
{
    pngle_t *pngle = pngle_new();

    if(pngle == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    int new_image_id = num_images + 1;

    image_t *new_image = images + new_image_id;

    pngle_set_user_data(pngle, new_image);
    pngle_set_init_callback(pngle, on_init);
    pngle_set_draw_callback(pngle, setpixel);

    int err = pngle_feed(pngle, png_data, png_size);

    if(err < 0) {
        ESP_LOGE(TAG, "PNGLE Error %d", err);
        return ESP_FAIL;
    }
    pngle_destroy(pngle);

    ESP_LOGI(TAG, "Decoded PNG id %d (%dx%d)", new_image_id, new_image->width, new_image->height);

    *out_image_id = new_image_id;
    num_images = new_image_id;

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

void image_blit_noclip(int source_image_id, uint16_t *lcd_buffer, vec2i const *src_pos, vec2i const *dst_pos, vec2i const *size)
{
    // lcd_buffer is fixed at LCD_WIDTH, LCD_HEIGHT !

    image_t const *source_image = image_get(source_image_id);

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

//////////////////////////////////////////////////////////////////////

void image_blit(int source_image_id, uint16_t *lcd_buffer, vec2i const *src_pos, vec2i const *dst_pos, vec2i const *size)
{
    vec2i sz = *size;
    vec2i s = *src_pos;
    vec2i d = *dst_pos;

    if(d.x < 0) {
        sz.x += d.x;
        s.x -= d.x;
        d.x = 0;
    }
    if(sz.x <= 0) {
        return;
    }
    if(d.y < 0) {
        sz.y += d.y;
        s.y -= d.y;
        d.y = 0;
    }
    if(sz.y <= 0) {
        return;
    }
    int right = (d.x + sz.x) - LCD_WIDTH;
    if(right > 0) {
        sz.x -= right;
        if(sz.x <= 0) {
            return;
        }
    }
    int bottom = (d.y + sz.y) - LCD_HEIGHT;
    if(bottom > 0) {
        sz.y -= bottom;
        if(sz.y <= 0) {
            return;
        }
    }
    image_blit_noclip(source_image_id, lcd_buffer, &s, &d, &sz);
}

//////////////////////////////////////////////////////////////////////

void image_fillrect_noclip(uint16_t *lcd_buffer, vec2i const *topleft, vec2i const *size, uint16_t pixel)
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

//////////////////////////////////////////////////////////////////////

void image_fillrect(uint16_t *lcd_buffer, vec2i const *topleft, vec2i const *size, uint16_t pixel)
{
    vec2i tl = *topleft;
    vec2i sz = *size;
    if(tl.x < 0) {
        sz.x += tl.x;
        tl.x = 0;
    }
    if(sz.x <= 0) {
        return;
    }
    if(tl.y < 0) {
        sz.y += tl.y;
        tl.y = 0;
    }
    if(sz.y <= 0) {
        return;
    }
    int right = (tl.x + sz.x) - LCD_WIDTH;
    if(right > 0) {
        sz.x -= right;
        if(sz.x <= 0) {
            return;
        }
    }
    int bottom = (tl.y + sz.y) - LCD_HEIGHT;
    if(bottom > 0) {
        sz.y -= bottom;
        if(sz.y <= 0) {
            return;
        }
    }
    image_fillrect_noclip(lcd_buffer, &tl, &sz, pixel);
}
