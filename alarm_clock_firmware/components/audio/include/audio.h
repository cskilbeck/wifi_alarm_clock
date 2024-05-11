#pragma once

#include <driver/gpio.h>
#include <driver/spi_master.h>

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

typedef struct vs1053_cfg_t
{
    spi_host_device_t spi_host;
    gpio_num_t pin_num_sclk;
    gpio_num_t pin_num_miso;
    gpio_num_t pin_num_mosi;
    gpio_num_t pin_num_cs;
    gpio_num_t pin_num_dcs;
    gpio_num_t pin_num_dreq;
    gpio_num_t pin_num_reset;
} vs1053_cfg_t;

//////////////////////////////////////////////////////////////////////

esp_err_t audio_init(vs1053_cfg_t const *cfg);

esp_err_t audio_set_volume(uint8_t volume);
esp_err_t audio_play();
esp_err_t audio_stop();

esp_err_t audio_acquire_buffer(size_t required, uint8_t **ptr, TickType_t ticks_to_wait);
esp_err_t audio_send_buffer(uint8_t *ptr);

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif
