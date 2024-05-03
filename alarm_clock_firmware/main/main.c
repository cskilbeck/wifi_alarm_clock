//////////////////////////////////////////////////////////////////////

#include <string.h>

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
#include "user_main.h"

#include "lcd_gc9a01.h"

#include "font/cascadia.h"
#include "font/segoe.h"

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

    image_t cascadia_font_image;
    image_decode_png(&cascadia_font_image, cascadia_png_start, cascadia_png_end - cascadia_png_start);

    image_t segoe_font_image;
    image_decode_png(&segoe_font_image, segoe_png_start, segoe_png_end - segoe_png_start);

    // image_t test_image;
    // image_decode_png(&test_image, test_png_start, test_png_end - test_png_start);

    uint16_t *lcd_buffer;

    if(lcd_get_backbuffer(&lcd_buffer, portMAX_DELAY) == ESP_OK) {

        vec2_t text_pos = { 32, 32 };
        font_drawtext(&Cascadia_font, &cascadia_font_image, lcd_buffer, &text_pos, (uint8_t const *)"Hello World!!", 0);

        text_pos.x = 0;
        text_pos.y = 120;
        font_drawtext(&Segoe_font, &segoe_font_image, lcd_buffer, &text_pos, (uint8_t const *)"Hello World!!", 0);

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