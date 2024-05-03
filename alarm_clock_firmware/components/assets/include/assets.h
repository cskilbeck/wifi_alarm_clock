#pragma once

#include <stdint.h>

extern const uint8_t Cascadia_png_start[] asm("_binary_Cascadia0_png_start");
extern const uint8_t Cascadia_png_end[] asm("_binary_Cascadia0_png_end");
#define Cascadia_png_size ((size_t)(Cascadia_png_end - Cascadia_png_start))

extern const uint8_t Segoe_png_start[] asm("_binary_Segoe0_png_start");
extern const uint8_t Segoe_png_end[] asm("_binary_Segoe0_png_end");
#define Segoe_png_size ((size_t)(Segoe_png_end - Segoe_png_start))

extern const uint8_t Digits_png_start[] asm("_binary_Digits0_png_start");
extern const uint8_t Digits_png_end[] asm("_binary_Digits0_png_end");
#define Digits_png_size ((size_t)(Digits_png_end - Digits_png_start))

extern const uint8_t test_png_start[] asm("_binary_test_png_start");
extern const uint8_t test_png_end[] asm("_binary_test_png_end");
#define test_png_size ((size_t)(test_png_end - test_png_start))

extern const uint8_t blip_png_start[] asm("_binary_blip_png_start");
extern const uint8_t blip_png_end[] asm("_binary_blip_png_end");
#define blip_png_size ((size_t)(blip_png_end - test_png_start))

extern const uint8_t small_blip_png_start[] asm("_binary_small_blip_png_start");
extern const uint8_t small_blip_png_end[] asm("_binary_small_blip_png_end");
#define small_blip_png_size ((size_t)(small_blip_png_end - small_blip_png_start))

#include "image.h"
#include "font.h"

#include "font/Cascadia.h"
#include "font/Segoe.h"
#include "font/Digits.h"
