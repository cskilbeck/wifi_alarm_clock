//////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

#define MAX_IMAGES 256

//////////////////////////////////////////////////////////////////////

extern const uint8_t test_png_start[] asm("_binary_test_png_start");
extern const uint8_t test_png_end[] asm("_binary_test_png_end");
#define test_png_size ((size_t)(test_png_end - test_png_start))

extern const uint8_t blip_png_start[] asm("_binary_blip_png_start");
extern const uint8_t blip_png_end[] asm("_binary_blip_png_end");
#define blip_png_size ((size_t)(blip_png_end - test_png_start))

extern const uint8_t small_blip_png_start[] asm("_binary_small_blip_png_start");
extern const uint8_t small_blip_png_end[] asm("_binary_small_blip_png_end");
#define small_blip_png_size ((size_t)(small_blip_png_end - small_blip_png_start))

extern const uint8_t face_png_start[] asm("_binary_face_png_start");
extern const uint8_t face_png_end[] asm("_binary_face_png_end");
#define face_png_size ((size_t)(face_png_end - face_png_start))

extern const uint8_t example_mp3_start[] asm("_binary_example_mp3_start");
extern const uint8_t example_mp3_end[] asm("_binary_example_mp3_end");
#define example_mp3_size ((size_t)(example_mp3_end - example_mp3_start))

extern const uint8_t boing_mp3_start[] asm("_binary_boing_mp3_start");
extern const uint8_t boing_mp3_end[] asm("_binary_boing_mp3_end");
#define boing_mp3_size ((size_t)(boing_mp3_end - boing_mp3_start))

//////////////////////////////////////////////////////////////////////

#include "image.h"
#include "font.h"

//////////////////////////////////////////////////////////////////////

extern font_handle_t cascadia_font;
extern font_handle_t segoe_font;
extern font_handle_t digits_font;
extern font_handle_t big_font;
extern font_handle_t forte_font;

//////////////////////////////////////////////////////////////////////

extern int blip_image_id;
extern int small_blip_image_id;
extern int face_image_id;
extern int test_image_id;

//////////////////////////////////////////////////////////////////////

#include "font/Cascadia.h"
#include "font/Segoe.h"
#include "font/Digits.h"
#include "font/Big.h"
#include "font/Forte.h"

//////////////////////////////////////////////////////////////////////

esp_err_t assets_init();

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif
