//////////////////////////////////////////////////////////////////////

#include <math.h>

#include <freertos/FreeRTOS.h>
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "util.h"
#include "display.h"
#include "font.h"
#include "image.h"
#include "led.h"
#include "lcd_gc9a01.h"
#include "ui.h"
#include "assets.h"
#include "encoder.h"

LOG_CONTEXT("ui");

//////////////////////////////////////////////////////////////////////

namespace
{

    int frame = 0;

    EventGroupHandle_t ui_timer_event_group_handle;
    TaskHandle_t ui_task_handle;

    //////////////////////////////////////////////////////////////////////

    void IRAM_ATTR ui_on_timer(void *)
    {
        xEventGroupSetBits(ui_timer_event_group_handle, 1);
    }

    //////////////////////////////////////////////////////////////////////

    void ui_task(void *)
    {
        // event group for the screen redraw timer

        ui_timer_event_group_handle = xEventGroupCreate();

        // setup the screen redraw timer

        esp_timer_handle_t ui_timer_handle;
        {
            esp_timer_create_args_t ui_timer_args = {};
            ui_timer_args.callback = ui_on_timer;
            ui_timer_args.dispatch_method = ESP_TIMER_TASK;
            ui_timer_args.skip_unhandled_events = false;
            ESP_ERROR_CHECK(esp_timer_create(&ui_timer_args, &ui_timer_handle));
            esp_timer_start_periodic(ui_timer_handle, 1000000 / 30);
        }

        // encoder setup

        encoder_handle_t encoder_handle;
        {
            encoder_config_t encoder_config = {};
            encoder_config.gpio_a = GPIO_NUM_1;
            encoder_config.gpio_b = GPIO_NUM_2;
            encoder_config.gpio_button = GPIO_NUM_42;
            ESP_ERROR_CHECK(encoder_init(&encoder_config, &encoder_handle));
        }

        // draw all the things

        bool draw_cls = true;
        bool draw_face = true;
        bool draw_time = true;
        bool draw_text = true;
        bool draw_seconds = true;

        int seconds = 0;

        while(true) {

            xEventGroupWaitBits(ui_timer_event_group_handle, 1, pdTRUE, pdFALSE, portMAX_DELAY);

            encoder_message_t msg;

            while(encoder_get_message(encoder_handle, &msg) == ESP_OK) {

                switch(msg) {

                case ENCODER_MSG_PRESS: {
                    size_t free_space = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
                    LOG_I("Free space: %u (%uKB)", free_space, free_space / 1024);
                    seconds = 60;
                } break;

                case ENCODER_MSG_RELEASE:
                    seconds = 0;
                    break;

                case ENCODER_MSG_ROTATE_CW:
                    seconds = min(60, seconds + 1);
                    break;

                case ENCODER_MSG_ROTATE_CCW:
                    seconds = max(0, seconds - 1);
                    break;

                default:
                    break;
                }
            }

            display_begin_frame();

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
}    // namespace

esp_err_t ui_init()
{
    if(xTaskCreatePinnedToCore(ui_task, "ui", 4096, NULL, 24, &ui_task_handle, 1) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
