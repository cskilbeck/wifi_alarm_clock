#pragma once

#include <esp_err.h>

#define LCD_WIDTH 240
#define LCD_HEIGHT 240

#if defined(__cplusplus)
extern "C" {
#endif

esp_err_t lcd_init();
esp_err_t lcd_update();
esp_err_t lcd_set_backlight(uint32_t brightness_0_8191);

esp_err_t lcd_get_backbuffer(uint16_t **buffer, TickType_t wait_for_ticks);
esp_err_t lcd_release_backbuffer();
esp_err_t lcd_release_backbuffer_and_update();

#if defined(__cplusplus)
}
#endif
