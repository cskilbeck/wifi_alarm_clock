//////////////////////////////////////////////////////////////////////

#include "assets.h"

#include "Cascadia.c"
#include "Segoe.c"
#include "Digits.c"
#include "Big.c"
#include "Forte.c"

//////////////////////////////////////////////////////////////////////

font_handle_t segoe_font;
font_handle_t cascadia_font;
font_handle_t digits_font;
font_handle_t big_font;
font_handle_t forte_font;

int blip_image_id;
int small_blip_image_id;
int face_image_id;

//////////////////////////////////////////////////////////////////////

esp_err_t assets_init()
{
    ESP_ERROR_CHECK(FONT_INIT(Cascadia, &cascadia_font));
    ESP_ERROR_CHECK(FONT_INIT(Segoe, &segoe_font));
    ESP_ERROR_CHECK(FONT_INIT(Digits, &digits_font));
    ESP_ERROR_CHECK(FONT_INIT(Big, &big_font));
    ESP_ERROR_CHECK(FONT_INIT(Forte, &forte_font));

    ESP_ERROR_CHECK(image_decode_png(&blip_image_id, blip_png_start, blip_png_size));
    ESP_ERROR_CHECK(image_decode_png(&small_blip_image_id, small_blip_png_start, small_blip_png_size));
    ESP_ERROR_CHECK(image_decode_png(&face_image_id, face_png_start, face_png_size));

    return ESP_OK;
}
