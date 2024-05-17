#pragma once

#include <driver/gpio.h>
#include <driver/spi_master.h>

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

esp_err_t audio_init();
esp_err_t audio_set_volume(uint8_t volume);
esp_err_t audio_play();
esp_err_t audio_stop();

esp_err_t audio_wait_for_sound_complete(TickType_t wait_for_ticks);

esp_err_t audio_acquire_buffer(size_t required, uint8_t **ptr, TickType_t ticks_to_wait);
esp_err_t audio_send_buffer(uint8_t *ptr);

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif
