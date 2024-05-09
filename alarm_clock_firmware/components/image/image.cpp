//////////////////////////////////////////////////////////////////////

#include <freertos/FreeRTOS.h>

#include <esp_log.h>
#include <esp_heap_caps.h>

#include "pngle.h"
#include "util.h"
#include "image.h"
#include "util.h"

static char const *TAG = "image";

//////////////////////////////////////////////////////////////////////

namespace
{
    int constexpr MAX_IMAGES = 128;

    image_t images[MAX_IMAGES];
    int num_images = 0;

    //////////////////////////////////////////////////////////////////////

    void setpixel(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
    {
        image_t *img = (image_t *)pngle_get_user_data(pngle);
        uint32_t r = rgba[0] << 16;
        uint32_t g = rgba[1] << 8;
        uint32_t b = rgba[2] << 0;
        uint32_t a = rgba[3] << 24;
        img->pixel_data[x + y * img->width] = a | r | g | b;
    }

    //////////////////////////////////////////////////////////////////////

    void on_init(pngle_t *pngle, uint32_t w, uint32_t h)
    {
        image_t *img = (image_t *)pngle_get_user_data(pngle);

        img->width = w;
        img->height = h;
        img->pixel_data = (uint32_t *)heap_caps_malloc(w * h * sizeof(uint32_t), MALLOC_CAP_SPIRAM);
        assert(img->pixel_data != NULL);
    }

}    // namespace

//////////////////////////////////////////////////////////////////////

image_t const *image_get(int image_id)
{
    if(image_id <= 0 || image_id > num_images) {
        return NULL;
    }
    return images + image_id;
}

//////////////////////////////////////////////////////////////////////

esp_err_t image_decode_png(int *out_image_id, uint8_t const *png_data, size_t png_size)
{
    pngle_t *pngle = pngle_new();

    if(pngle == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    int new_image_id = num_images + 1;

    image_t *new_image = images + new_image_id;

    pngle_set_user_data(pngle, new_image);
    pngle_set_init_callback(pngle, on_init);
    pngle_set_draw_callback(pngle, setpixel);

    int err = pngle_feed(pngle, png_data, png_size);

    if(err < 0) {
        ESP_LOGE(TAG, "PNGLE Error %d", err);
        return ESP_FAIL;
    }
    pngle_destroy(pngle);

    ESP_LOGI(TAG, "Decoded PNG id %d (%dx%d)", new_image_id, new_image->width, new_image->height);

    new_image->image_id = new_image_id;
    *out_image_id = new_image_id;
    num_images = new_image_id;

    return ESP_OK;
}
