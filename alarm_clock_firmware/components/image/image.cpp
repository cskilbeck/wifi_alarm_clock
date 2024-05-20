//////////////////////////////////////////////////////////////////////

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_log.h>
#include <esp_heap_caps.h>

#include "pngle.h"
#include "util.h"
#include "image.h"
#include "util.h"

LOG_CONTEXT("image");

//////////////////////////////////////////////////////////////////////

namespace
{
    int constexpr MAX_IMAGES = 64;

    EXT_RAM_BSS_ATTR image_t images[MAX_IMAGES];
    int num_images = 0;

    SemaphoreHandle_t image_semaphore;

    //////////////////////////////////////////////////////////////////////

    void setpixel(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
    {
        image_t *img = (image_t *)pngle_get_user_data(pngle);
        uint32_t r = rgba[0] << 16;
        uint32_t g = rgba[1] << 8;
        uint32_t b = rgba[2] << 0;
        uint32_t a = rgba[3] << 24;
        uint32_t *p = const_cast<uint32_t *>(img->pixel_data);    // YOINK!
        p[x + y * img->width] = a | r | g | b;
    }

    //////////////////////////////////////////////////////////////////////

    void on_init(pngle_t *pngle, uint32_t w, uint32_t h)
    {
        image_t *img = (image_t *)pngle_get_user_data(pngle);

        img->width = w;
        img->height = h;
        img->pixel_data = (uint32_t const *)heap_caps_malloc(w * h * sizeof(uint32_t), MALLOC_CAP_SPIRAM);
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

image_t const *image_get_unchecked(int image_id)
{
    return images + image_id;
}

//////////////////////////////////////////////////////////////////////

esp_err_t image_decode_png(char const *name, int *out_image_id, uint8_t const *png_data, size_t png_size)
{
    LOG_I("%s", name);

    pngle_t *pngle = pngle_new();

    if(pngle == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    image_t temp_image;

    pngle_set_user_data(pngle, &temp_image);
    pngle_set_init_callback(pngle, on_init);
    pngle_set_draw_callback(pngle, setpixel);

    int err = pngle_feed(pngle, png_data, png_size);

    if(err < 0) {
        LOG_E("PNGLE Error %d", err);
        heap_caps_free(const_cast<void *>(reinterpret_cast<void const *>(temp_image.pixel_data)));
        return ESP_FAIL;
    }
    pngle_destroy(pngle);

    xSemaphoreTake(image_semaphore, portMAX_DELAY);
    image_t *new_image = images + num_images;
    *new_image = temp_image;
    new_image->image_id = num_images;
    num_images += 1;
    xSemaphoreGive(image_semaphore);

    LOG_D("Decoded PNG id %d (%dx%d)", temp_image.image_id, temp_image.width, temp_image.height);

    *out_image_id = new_image->image_id;

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t image_init()
{
    image_semaphore = xSemaphoreCreateMutex();
    return ESP_OK;
}
