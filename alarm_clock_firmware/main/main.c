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

LOG_TAG("main");

//////////////////////////////////////////////////////////////////////

#define ERASE_FLASH 0

int frame = 0;

EventGroupHandle_t display_event_group_handle;
TaskHandle_t display_task_handle;

void display_on_timer(void *)
{
    xEventGroupSetBits(display_event_group_handle, 1);
}

void display_task(void *)
{
    display_event_group_handle = xEventGroupCreate();

    esp_timer_create_args_t display_timer_args = {};
    display_timer_args.callback = display_on_timer;
    display_timer_args.dispatch_method = ESP_TIMER_TASK;
    display_timer_args.skip_unhandled_events = true;
    esp_timer_handle_t display_timer_handle;
    ESP_ERROR_CHECK(esp_timer_create(&display_timer_args, &display_timer_handle));

    esp_timer_start_periodic(display_timer_handle, 1000000 / 20);

    bool draw_cls = true;
    bool draw_face = true;
    bool draw_time = true;
    bool draw_text = true;
    bool draw_seconds = true;

    while(true) {

        xEventGroupWaitBits(display_event_group_handle, 1, 1, 0, portMAX_DELAY);

        display_begin_frame();

        int seconds = (int)(esp_timer_get_time() / 50000llu) % 60;

        //////////////////////////////////////////////////////////////////////

        if(draw_cls) {
            vec2i dst_pos = { 0, 0 };
            vec2i size = { LCD_WIDTH, LCD_HEIGHT };
            display_fillrect(&dst_pos, &size, 0xff3f003f, blend_opaque);
        }

        //////////////////////////////////////////////////////////////////////

        if(draw_face) {
            float speed = 0.025f;
            float t = frame * speed;

            int x = (int)(cosf(t) * 90) + LCD_WIDTH / 2;
            int y = (int)(sinf(t) * 90) + LCD_HEIGHT / 2;

            vec2i dst_pos = { x, y };
            vec2f pivot = { 0.5f, 0.5f };
            display_image(&dst_pos, face_image_id, 255, blend_multiply, &pivot);
        }

        //////////////////////////////////////////////////////////////////////

        if(draw_time) {
            font_handle_t f = digits_font;

            char time[7];
            snprintf(time, 7, "23:%02d", seconds);

            uint8_t const *text = (uint8_t const *)time;
            vec2i text_size;

            font_measure_string(f, text, &text_size);

            int x = LCD_WIDTH / 2;
            int y = LCD_HEIGHT / 2;

            vec2i text_pos = { x - text_size.x / 2, y - text_size.y / 2 };
            font_drawtext(f, &text_pos, text, 255, blend_multiply);
        }

        //////////////////////////////////////////////////////////////////////

        if(draw_text) {
            font_handle_t f = forte_font;

            uint8_t const *text = (uint8_t const *)"Hello, World!";

            vec2i text_size;

            font_measure_string(f, text, &text_size);

            float speed = 0.05f;
            float t = frame * speed;

            int x = (int)(sinf(t) * 100) + 120;
            int y = (int)(cosf(t) * 100) + 120;

            vec2i text_pos = { x - text_size.x / 2, y - text_size.y / 2 };
            font_drawtext(f, &text_pos, text, 255, blend_multiply);
        }

        //////////////////////////////////////////////////////////////////////

        if(draw_seconds) {
            image_t const *src_img = image_get(blip_image_id);
            for(int i = 0; i < seconds; ++i) {
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
        }

        display_end_frame();

        frame += 1;
    }
}

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

    display_init();

    assets_init();

    ESP_LOGI(TAG, "lcd_update complete");

    int64_t t = esp_timer_get_time();
    int64_t now;

    int64_t fade_time_uS = 500000ll;

    xTaskCreatePinnedToCore(display_task, "display", 4096, NULL, 2, &display_task_handle, 0);

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
