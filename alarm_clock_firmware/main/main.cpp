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

#include "board.h"

#include "led.h"
#include "encoder.h"
#include "stream.h"
#include "image.h"
#include "assets.h"
#include "font.h"
#include "assets.h"
#include "lcd_gc9a01.h"
#include "audio.h"
#include "display.h"
#include "ui.h"

LOG_CONTEXT("main");

//////////////////////////////////////////////////////////////////////

#define ERASE_FLASH 0

//////////////////////////////////////////////////////////////////////

void play_song(void *)
{
    uint8_t const *src = example_mp3_start;
    // size_t remain = example_mp3_size;
    size_t remain = 200000;

    LOG_I("Play song %u bytes", example_mp3_size);

    audio_play();

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
    size_t free_space = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    LOG_I("Free space: %u (%uKB)", free_space, free_space / 1024);
    audio_stop();
    vTaskDelay(portMAX_DELAY);
}

//////////////////////////////////////////////////////////////////////

extern "C" void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    LOG_I("app_main");

    led_init();
    led_set_on();

    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || ERASE_FLASH) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_netif_init());

    vs1053_cfg_t cfg = {};
    cfg.pin_num_cs = GPIO_NUM_21;
    cfg.pin_num_dcs = GPIO_NUM_38;
    cfg.pin_num_dreq = GPIO_NUM_47;
    cfg.pin_num_miso = GPIO_NUM_41;
    cfg.pin_num_mosi = GPIO_NUM_48;
    cfg.pin_num_reset = GPIO_NUM_39;
    cfg.pin_num_sclk = GPIO_NUM_40;
    cfg.spi_host = SPI3_HOST;

    audio_init(&cfg);

    display_init();

    assets_init();

    xTaskCreatePinnedToCore(play_song, "play_song", 4096, nullptr, 5, nullptr, 0);

    int64_t t = esp_timer_get_time();
    int64_t now;

    int64_t fade_time_uS = 500000ll;

    ui_init();

    while((now = esp_timer_get_time() - t) <= fade_time_uS) {
        uint32_t b = min(8191lu, (uint32_t)(now * 9500 / fade_time_uS));
        lcd_set_backlight(b);
        vTaskDelay(1);
    }

    led_set_off();
}
