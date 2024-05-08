//////////////////////////////////////////////////////////////////////

#pragma once

#include <esp_err.h>

//////////////////////////////////////////////////////////////////////

#define LCD_WIDTH 240
#define LCD_HEIGHT 240

#define LCD_BITS_PER_PIXEL 18

// FORMAT for 18bpp is like 24bpp but low two bits are ignored
#if LCD_BITS_PER_PIXEL == 18
#define LCD_BYTES_PER_PIXEL 3
#else
#define LCD_BYTES_PER_PIXEL 2
#endif

#define LCD_BYTES_PER_LINE (LCD_WIDTH * LCD_BYTES_PER_PIXEL)

#define LCD_SECTION_HEIGHT 16    // largest power of 2 which is divisible into 240
#define LCD_NUM_SECTIONS (LCD_HEIGHT / LCD_SECTION_HEIGHT)

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

typedef void (*lcd_buffer_filler)(int section, uint8_t *buffer);

esp_err_t lcd_init();
esp_err_t lcd_update(lcd_buffer_filler buffer_filler);
esp_err_t lcd_set_backlight(uint32_t brightness_0_8191);

//////////////////////////////////////////////////////////////////////

#define COLOR_BLACK 0xff000000
#define COLOR_BLUE 0xff0000ff
#define COLOR_GREEN 0xff00ff00
#define COLOR_CYAN (COLOR_BLUE | COLOR_GREEN)
#define COLOR_RED 0xffff0000
#define COLOR_MAGENTA (COLOR_RED | COLOR_BLUE)
#define COLOR_YELLOW (COLOR_RED | COLOR_GREEN)
#define COLOR_WHITE (COLOR_RED | COLOR_GREEN | COLOR_BLUE)

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif
