//////////////////////////////////////////////////////////////////////

#include <esp_log.h>
#include "assets.h"

#include "Cascadia.c"
#include "Segoe.c"
#include "Digits.c"
#include "Big.c"
#include "Forte.c"

LOG_CONTEXT("assets");

#define LOAD_IMG(x) ESP_ERROR_CHECK(image_decode_png(#x, &image_id_##x, x##_png_start, x##_png_size))

//////////////////////////////////////////////////////////////////////

font_handle_t segoe_font;
font_handle_t cascadia_font;
font_handle_t digits_font;
font_handle_t big_font;
font_handle_t forte_font;

int image_id_blip;
int image_id_small_blip;
int image_id_face;
int image_id_test;
int image_id_world;

//////////////////////////////////////////////////////////////////////

esp_err_t assets_init()
{
    LOG_I("begin");

    ESP_ERROR_CHECK(FONT_INIT(Cascadia, &cascadia_font));
    ESP_ERROR_CHECK(FONT_INIT(Segoe, &segoe_font));
    ESP_ERROR_CHECK(FONT_INIT(Digits, &digits_font));
    ESP_ERROR_CHECK(FONT_INIT(Big, &big_font));
    ESP_ERROR_CHECK(FONT_INIT(Forte, &forte_font));

    LOAD_IMG(blip);
    LOAD_IMG(small_blip);
    LOAD_IMG(face);
    LOAD_IMG(test);
    LOAD_IMG(world);

    LOG_I("end");

    return ESP_OK;
}
