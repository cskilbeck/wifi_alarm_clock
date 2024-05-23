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

#include <driver/gpio.h>
#include <driver/spi_master.h>

#include "esp_http_client.h"

#include "util.h"
#include "led.h"
#include "encoder.h"
#include "image.h"
#include "assets.h"
#include "font.h"
#include "assets.h"
#include "lcd_gc9a01.h"
#include "audio.h"
#include "display.h"
#include "ui.h"
#include "wifi.h"

#include "http_stream.h"
#include "hls_parse.h"
#include "hls_playlist.h"

LOG_CONTEXT("main");

//////////////////////////////////////////////////////////////////////

#define ERASE_FLASH 0

namespace
{
    int constexpr UI_FPS = 30;

    TaskHandle_t main_ui_task_handle;
    EventGroupHandle_t frame_timer_event_group_handle;
    encoder_handle_t encoder_handle;
    int frame = 0;

    float rotation = 0;
    float rotation_vel = 0;

    int volume = 0x40;

}    // namespace

//////////////////////////////////////////////////////////////////////

void play_file(void *)
{
    audio_wait_for_initialization_complete(portMAX_DELAY);

    uint8_t const *src = boing_mp3_start;
    size_t remain = boing_mp3_size;
    // size_t remain = 200000;

    LOG_I("Play song %u bytes", boing_mp3_size);

    if(audio_play() == ESP_OK) {
        while(remain != 0) {
            uint8_t *buffer;
            size_t fragment = min(2048u, remain);
            audio_acquire_buffer(fragment, &buffer, portMAX_DELAY);
            if(buffer != nullptr) {
                memcpy(buffer, src, fragment);
                audio_send_buffer(buffer);
                src += fragment;
                remain -= fragment;
            } else {
                LOG_E("Huh?");
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
        audio_wait_for_send_complete(portMAX_DELAY);
        LOG_I("Play song complete");
        audio_stop();
        audio_wait_for_sound_complete(portMAX_DELAY);
    }

    vTaskDelete(nullptr);
}

//////////////////////////////////////////////////////////////////////

char buffer[16384];

void play_station()
{
    http_stream_cfg_t stream_config;
    HTTP_STREAM_CFG_SET_DEFAULT(stream_config);

    stream_config.enable_playlist_parser = true;
    // stream_config.request_range_size = 1;
    http_stream_handle_t stream = http_stream_init(&stream_config);

    // #define STREAM_URI "http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_radio_fourfm/bbc_radio_fourfm.isml/bbc_radio_fourfm-audio=320000.m3u8"
    // #define STREAM_URI "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service"
    // #define STREAM_URI "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3"
    // #define STREAM_URI "http://media-ice.musicradio.com/LBCLondon.m3u"
    // #define STREAM_URI "http://media-ice.musicradio.com/LBCUK.m3u"
    // #define STREAM_URI "http://media-ice.musicradio.com/LBCUKMP3Low"

#define STREAM_URI "http://www.samisite.com/sound/cropShadesofGrayMonkees.mp3"

    esp_err_t ret = http_stream_open(stream, STREAM_URI);
    if(ret != ESP_OK) {
        LOG_E("http open ERROR %d:%s", ret, esp_err_to_name(ret));
    } else {
        if(audio_play() == ESP_OK) {
            while(true) {
                int got = http_stream_read(stream, buffer, sizeof(buffer), portMAX_DELAY, nullptr);
                LOG_I("%08lx", *reinterpret_cast<uint32_t *>(buffer));
                if(got <= 0) {
                    break;
                }
                uint8_t *audio_buffer;
                audio_acquire_buffer(got, &audio_buffer, portMAX_DELAY);
                if(audio_buffer != nullptr) {
                    memcpy(audio_buffer, buffer, got);
                    audio_send_buffer(audio_buffer);
                } else {
                    LOG_E("Huh?");
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            }
            audio_wait_for_sound_complete(portMAX_DELAY);
            audio_stop();
        }
    }
}

//////////////////////////////////////////////////////////////////////

ui_draw_item_handle_t ui_item_time;

unsigned seconds = 0;
int alpha = 255;

ui_input_handler_status ui_handler(int frame)
{
    encoder_message_t msg;

    if((frame & 7) == 0) {
        seconds = (seconds + 1) % 60;
    }

    while(encoder_get_message(encoder_handle, &msg) == ESP_OK) {

        switch(msg) {

        case ENCODER_MSG_PRESS: {
            size_t free_space = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
            LOG_I("Free space: %u (%uKB)", free_space, free_space / 1024);
            ui_item_toggle_flags(ui_item_time, uif_hidden);
        } break;

        case ENCODER_MSG_RELEASE:
            break;

        case ENCODER_MSG_ROTATE_CW:
            alpha = min(255, alpha + 8);
            rotation_vel += 2;
            volume = max(1, volume - 1);
            audio_set_volume((uint8_t)volume);
            break;

        case ENCODER_MSG_ROTATE_CCW:
            alpha = max(0, alpha - 8);
            rotation_vel -= 2;
            volume = min(255, volume + 1);
            audio_set_volume((uint8_t)volume);
            break;

        default:
            break;
        }
    }
    return ui_input_handler_keep;
}

//////////////////////////////////////////////////////////////////////

void draw_cls(int frame)
{
    vec2i dst_pos = { 0, 0 };
    vec2i size = { LCD_WIDTH, LCD_HEIGHT };
    display_fillrect(&dst_pos, &size, 0xff3f003f, blend_opaque);
}

//////////////////////////////////////////////////////////////////////

void draw_face(int frame)
{
    float speed = 0.025f;
    float t = frame * speed;

    int x = (int)(cosf(t) * 90) + LCD_WIDTH / 2;
    int y = (int)(sinf(t) * 90) + LCD_HEIGHT / 2;

    vec2i dst_pos = { x, y };
    vec2f pivot = { 0.5f, 0.5f };
    display_image(&dst_pos, image_id_face, alpha, blend_multiply, &pivot);
}

//////////////////////////////////////////////////////////////////////

void draw_time(int frame)
{
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

void draw_text(int frame)
{
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

void draw_seconds(int frame)
{
    image_t const *src_img = image_get(image_id_blip);
    for(int i = 0; i <= seconds; ++i) {
        float t = (float)i * M_TWOPI / 60.0f;
        float x = sinf(t) * 114 + 120;
        float y = 120 - cosf(t) * 114;
        vec2i src_pos = { 0, 0 };
        vec2i size = { src_img->width, src_img->height };
        vec2i dst_pos = { (int)x, (int)y };
        dst_pos.x -= size.x / 2;
        dst_pos.y -= size.y / 2;
        display_imagerect(&dst_pos, &src_pos, &size, image_id_blip, 0xff, blend_add);
    }

    src_img = image_get(image_id_small_blip);
    for(int i = 0; i < 60; i += 5) {
        float t = (float)i * M_TWOPI / 60.0f;
        float x = sinf(t) * 104 + 120;
        float y = 120 - cosf(t) * 104;
        vec2i src_pos = { 0, 0 };
        vec2i size = { src_img->width, src_img->height };
        vec2i dst_pos = { (int)x, (int)y };
        dst_pos.x -= size.x / 2;
        dst_pos.y -= size.y / 2;
        display_imagerect(&dst_pos, &src_pos, &size, image_id_small_blip, 0xff, blend_add);
    }
}

//////////////////////////////////////////////////////////////////////

void IRAM_ATTR frame_on_timer(void *)
{
    xEventGroupSetBits(frame_timer_event_group_handle, 1);
}

//////////////////////////////////////////////////////////////////////

void main_ui_task(void *)
{
    // encoder setup
    encoder_config_t encoder_config = {};
    encoder_config.gpio_a = GPIO_NUM_1;
    encoder_config.gpio_b = GPIO_NUM_2;
    encoder_config.gpio_button = GPIO_NUM_42;
    ESP_ERROR_CHECK(encoder_init(&encoder_config, &encoder_handle));

    ui_init();

    frame_timer_event_group_handle = xEventGroupCreate();

    // setup the screen redraw timer

    esp_timer_handle_t ui_timer_handle;
    {
        esp_timer_create_args_t ui_timer_args = {};
        ui_timer_args.callback = frame_on_timer;
        ui_timer_args.dispatch_method = ESP_TIMER_TASK;
        ui_timer_args.skip_unhandled_events = false;
        ESP_ERROR_CHECK(esp_timer_create(&ui_timer_args, &ui_timer_handle));
        esp_timer_start_periodic(ui_timer_handle, 1000000 / UI_FPS);
    }

    // ui_add_item(ui_draw_priority_0, draw_cls);
    // ui_add_item(ui_draw_priority_0, draw_seconds);
    // ui_add_item(ui_draw_priority_0, draw_text);

    // ui_item_time = ui_add_item(ui_draw_priority_7, draw_time);

    // ui_add_item(ui_draw_priority_6, draw_face);

    ui_push_input_handler(ui_handler);

    // main UI loop - handle encoder messages and draw all the things

    while(true) {

        xEventGroupWaitBits(frame_timer_event_group_handle, 1, pdTRUE, pdFALSE, portMAX_DELAY);

        ui_input_handler current_handler = ui_get_current_handler();

        if(current_handler != nullptr && current_handler(frame) == ui_input_handler_pop) {
            ui_pop_current_handler();
        }

        display_begin_frame();

        // ui_draw(frame);

        rotation += rotation_vel;

        while(rotation < 0) {
            rotation += 480.0f;
        }

        while(rotation >= 480.0f) {
            rotation -= 480.0f;
        }

        rotation_vel *= 0.9f;

        display_sphere((int)rotation, image_id_world, 255, blend_opaque);

        display_end_frame();

        frame += 1;
    }
}

//////////////////////////////////////////////////////////////////////

extern "C" void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    LOG_I("app_main");

    led_init();
    led_set_off();

    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || ERASE_FLASH) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_netif_init());

    image_init();
    audio_init();
    assets_init();
    display_init();
    wifi_init();

    LOG_I("Audio init complete");

    audio_wait_for_initialization_complete(portMAX_DELAY);

    xTaskCreatePinnedToCore(play_file, "play_file", 3072, nullptr, 15, nullptr, 1);

    xTaskCreatePinnedToCore(main_ui_task, "main_ui", 4096, NULL, 15, &main_ui_task_handle, 1);

    int64_t t = esp_timer_get_time();
    int64_t now;
    int64_t fade_time_uS = 2000000ll;

    while((now = esp_timer_get_time() - t) <= fade_time_uS) {
        uint32_t b = min(8191lu, (uint32_t)(now * 9500 / fade_time_uS));
        lcd_set_backlight(b);
        vTaskDelay(1);
    }

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, pdFALSE, pdTRUE, portMAX_DELAY);

    play_station();
}
