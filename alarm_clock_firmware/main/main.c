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

static char const *TAG = "main";

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

    uint16_t *lcd_buffer;

    if(lcd_get_backbuffer(&lcd_buffer, portMAX_DELAY) == ESP_OK) {

        uint8_t const *text = (uint8_t const *)"22:48";

        vec2 text_size;

        font_measure_string(digits_font, text, &text_size);

        vec2 text_pos = { (240 - text_size.x) / 2, (240 - text_size.y) / 2 };
        font_drawtext(digits_font, lcd_buffer, &text_pos, text, COLOR_BLACK);

        image_t const *src_img = image_get(blip_image_id);
        for(int i = 0; i < 23; ++i) {
            float t = (float)i * M_TWOPI / 60.0f;
            float x = sinf(t) * 114 + 120;
            float y = 120 - cosf(t) * 114;
            vec2 src_pos = { 0, 0 };
            vec2 size = { src_img->width, src_img->height };
            vec2 dst_pos = { (int)x, (int)y };
            dst_pos.x -= size.x / 2;
            dst_pos.y -= size.y / 2;
            image_blit(blip_image_id, lcd_buffer, &src_pos, &dst_pos, &size);
        }

        src_img = image_get(small_blip_image_id);
        for(int i = 0; i < 60; i += 5) {
            float t = (float)i * M_TWOPI / 60.0f;
            float x = sinf(t) * 104 + 120;
            float y = 120 - cosf(t) * 104;
            vec2 src_pos = { 0, 0 };
            vec2 size = { src_img->width, src_img->height };
            vec2 dst_pos = { (int)x, (int)y };
            dst_pos.x -= size.x / 2;
            dst_pos.y -= size.y / 2;
            image_blit(small_blip_image_id, lcd_buffer, &src_pos, &dst_pos, &size);
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

    led_set_off();

    stream_init();
}
