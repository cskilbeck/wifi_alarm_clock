//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "board.h"

#include "led.h"
#include "encoder.h"
#include "stream.h"
#include "image.h"
#include "assets.h"
#include "font.h"
#include "assets.h"

#include "lcd_gc9a01.h"

static const char *TAG = "main";

//////////////////////////////////////////////////////////////////////

#define ERASE_FLASH 0

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "app_main");

    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || ERASE_FLASH) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_netif_init());

    led_init();
    lcd_init();

    font_handle_t cascadia_font;
    font_handle_t segoe_font;
    font_handle_t digits_font;

    ESP_ERROR_CHECK(FONT_INIT(Cascadia, &cascadia_font));
    ESP_ERROR_CHECK(FONT_INIT(Segoe, &segoe_font));
    ESP_ERROR_CHECK(FONT_INIT(Digits, &digits_font));

    image_t blip;
    image_decode_png(&blip, blip_png_start, blip_png_size);

    image_t small_blip;
    image_decode_png(&small_blip, small_blip_png_start, small_blip_png_size);

    uint16_t *lcd_buffer;

    if(lcd_get_backbuffer(&lcd_buffer, portMAX_DELAY) == ESP_OK) {

        uint8_t const *text = (uint8_t const *)"22:48";

        vec2 text_size;

        font_measure_string(digits_font, text, &text_size);

        vec2 text_pos = { (240 - text_size.x) / 2, (240 - text_size.y) / 2 };
        font_drawtext(digits_font, lcd_buffer, &text_pos, text, COLOR_BLACK);

        for(int i = 0; i < 23; ++i) {
            float t = (float)i * M_TWOPI / 60.0f;
            float x = sinf(t) * 114 + 120;
            float y = 120 - cosf(t) * 114;
            vec2 src_pos = { 0, 0 };
            image_t *src_img = &blip;
            vec2 size = { src_img->width, src_img->height };
            vec2 dst_pos = { (int)x, (int)y };
            dst_pos.x -= size.x / 2;
            dst_pos.y -= size.y / 2;
            image_blit(src_img, lcd_buffer, &src_pos, &dst_pos, &size);
        }

        for(int i = 0; i < 60; i += 5) {
            float t = (float)i * M_TWOPI / 60.0f;
            float x = sinf(t) * 104 + 120;
            float y = 120 - cosf(t) * 104;
            vec2 src_pos = { 0, 0 };
            image_t *src_img = &small_blip;
            vec2 size = { src_img->width, src_img->height };
            vec2 dst_pos = { (int)x, (int)y };
            dst_pos.x -= size.x / 2;
            dst_pos.y -= size.y / 2;
            image_blit(src_img, lcd_buffer, &src_pos, &dst_pos, &size);
        }

        lcd_release_backbuffer_and_update();
    }

    int64_t t = esp_timer_get_time();
    int64_t now;

    int64_t fade_time_uS = 500000ll;

    while((now = esp_timer_get_time() - t) <= fade_time_uS) {
        uint32_t b = (uint32_t)(now * 9500 / fade_time_uS);
        if(b > 8191) {
            b = 8191;
        }
        lcd_set_backlight(b);
        vTaskDelay(1);
    }

    stream_init();
}