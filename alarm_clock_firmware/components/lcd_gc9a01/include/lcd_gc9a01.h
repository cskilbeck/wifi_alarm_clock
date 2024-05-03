//////////////////////////////////////////////////////////////////////

#pragma once

#include <esp_err.h>

//////////////////////////////////////////////////////////////////////

#define LCD_WIDTH 240
#define LCD_HEIGHT 240

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

esp_err_t lcd_init();
esp_err_t lcd_update();
esp_err_t lcd_set_backlight(uint32_t brightness_0_8191);

esp_err_t lcd_get_backbuffer(uint16_t **buffer, TickType_t wait_for_ticks);
esp_err_t lcd_release_backbuffer();
esp_err_t lcd_release_backbuffer_and_update();

//////////////////////////////////////////////////////////////////////

#define COLOR_BLACK 0
#define COLOR_RED (31 << 11)
#define COLOR_GREEN (63 << 5)
#define COLOR_BLUE 31
#define COLOR_CYAN (COLOR_BLUE | COLOR_GREEN)
#define COLOR_MAGENTA (COLOR_RED | COLOR_BLUE)
#define COLOR_YELLOW (COLOR_RED | COLOR_GREEN)
#define COLOR_WHITE (COLOR_RED | COLOR_GREEN | COLOR_BLUE)

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif
