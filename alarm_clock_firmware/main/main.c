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
#include "display.h"

static char const *TAG = "main";

//////////////////////////////////////////////////////////////////////

#define ERASE_FLASH 0

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "app_main");

    led_init();
    led_set_on();

    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || ERASE_FLASH) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_netif_init());

    lcd_init();
    assets_init();

    display_reset();

    /////
    {
        vec2i dst_pos = { 0, 0 };
        vec2i size = { LCD_WIDTH, LCD_HEIGHT };
        display_fillrect(&dst_pos, &size, 0xffff00ff, blend_opaque);
    }

    /////
    {
        vec2i dst_pos = { 20, 20 };
        vec2i src_pos = { 0, 0 };
        vec2i size = { 64, 64 };
        display_imagerect(&dst_pos, &src_pos, &size, segoe_font->image_index, 200, blend_multiply);
    }

    /////
    {
        font_handle_t f = forte_font;

        uint8_t const *text = (uint8_t const *)"Hello, World!";

        vec2i text_size;

        font_measure_string(f, text, &text_size);

        vec2i text_pos = { (240 - text_size.x) / 2, (240 - text_size.y) / 2 };
        font_drawtext(f, &text_pos, text, 255, blend_multiply);
    }

    image_t const *src_img = image_get(blip_image_id);
    for(int i = 0; i < 23; ++i) {
        float t = (float)i * M_TWOPI / 60.0f;
        float x = sinf(t) * 114 + 120;
        float y = 120 - cosf(t) * 114;
        vec2i src_pos = { 0, 0 };
        vec2i size = { src_img->width, src_img->height };
        vec2i dst_pos = { (int)x, (int)y };
        dst_pos.x -= size.x / 2;
        dst_pos.y -= size.y / 2;
        display_imagerect(&dst_pos, &src_pos, &size, blip_image_id, 0xff, blend_add);
    }

    src_img = image_get(small_blip_image_id);
    for(int i = 0; i < 60; i += 5) {
        float t = (float)i * M_TWOPI / 60.0f;
        float x = sinf(t) * 104 + 120;
        float y = 120 - cosf(t) * 104;
        vec2i src_pos = { 0, 0 };
        vec2i size = { src_img->width, src_img->height };
        vec2i dst_pos = { (int)x, (int)y };
        dst_pos.x -= size.x / 2;
        dst_pos.y -= size.y / 2;
        display_imagerect(&dst_pos, &src_pos, &size, small_blip_image_id, 0xff, blend_add);
    }

    lcd_update();

    ESP_LOGI(TAG, "lcd_update complete");

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

    led_set_off();

    vTaskDelay(portMAX_DELAY);

    stream_init();
}
