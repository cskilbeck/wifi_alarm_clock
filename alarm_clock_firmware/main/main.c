//////////////////////////////////////////////////////////////////////

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_peripherals.h"
#include "esp_decoder.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "audio_alc.h"

#include "i2s_stream.h"
#include "http_stream.h"
#include "mp3_decoder.h"

#include "periph_touch.h"
#include "periph_adc_button.h"
#include "periph_button.h"
#include "periph_wifi.h"

#include "board.h"

#include "led.h"
#include "lcd.h"
#include "wifi.h"
#include "encoder.h"
#include "stream.h"
#include "image.h"
#include "assets.h"
#include "font.h"
#include "user_main.h"

#include "font/cascadia.h"

static const char *TAG = "main";

//////////////////////////////////////////////////////////////////////

QueueHandle_t main_message_queue;

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_netif_init());

    led_init();
    lcd_init();

    lcd_set_backlight(8191);

    image_t font_image;
    image_decode_png(&font_image, cascadia_png_start, cascadia_png_end - cascadia_png_start);
    // image_decode_png(&font_image, test_png_start, test_png_end - test_png_start);

    uint16_t *lcd_buffer;

    if(lcd_get_backbuffer(&lcd_buffer, portMAX_DELAY) == ESP_OK) {

        vec2_t text_pos = { 32, 32 };
        font_drawtext(&Cascadia_font, &font_image, lcd_buffer, &text_pos, (uint8_t const *)"Hello World!!");

        lcd_release_backbuffer_and_update();
    }

    wifi_init();

    ESP_LOGI(TAG, "?");

    main_message_queue = xQueueCreate(4, sizeof(main_message_t));

    wifi_connect();

    ESP_LOGI(TAG, "main loop");

    while(1) {
        main_message_t msg;

        ESP_LOGI(TAG, "Waiting...");

        xQueueReceive(main_message_queue, &msg, portMAX_DELAY);

        ESP_LOGI(TAG, "got msg %d", msg.code);

        switch(msg.code) {

        case main_message_code_wifi_connected:
            stream_init();
            break;

        case main_message_code_wifi_disconnected:
            break;

        case main_message_code_user_input: {
            int direction = (int)msg.param[0];
            ESP_LOGI(TAG, "Direction: %d", direction);
        } break;

        default:
            ESP_LOGI(TAG, "HUH? msg: %d", msg.code);
            break;
        }
    }
}