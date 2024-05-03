/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_log.h"
#include "mywifi.h"
#include "periph_mywifi.h"
#include "audio_mem.h"

static const char *TAG = "periph_mywifi";

#define VALIDATE_ENCODER(periph, ret)                                  \
    if(!(periph && esp_periph_get_id(periph) == PERIPH_ID_MYWIFI)) {   \
        ESP_LOGE(TAG, "Invalid ENCODER periph, at line %d", __LINE__); \
        return ret;                                                    \
    }

typedef struct
{
    esp_mywifi_handle_t mywifi_handle;

} periph_mywifi_t;

static esp_err_t _mywifi_init(esp_periph_handle_t self)
{
    ESP_LOGI(TAG, "init");

    VALIDATE_ENCODER(self, ESP_FAIL);
    periph_mywifi_t *periph_mywifi = esp_periph_get_data(self);

    mywifi_config_t mywifi_config = {};

    return mywifi_init(&mywifi_config, &periph_mywifi->mywifi_handle);
}

static esp_err_t _mywifi_run(esp_periph_handle_t self, audio_event_iface_msg_t *msg)
{
    ESP_LOGI(TAG, "RUN!");
    // periph_mywifi_t *periph_mywifi = esp_periph_get_data(self);
    esp_periph_send_event(self, msg->cmd, NULL, 0);
    return ESP_OK;
}

static esp_err_t _mywifi_destroy(esp_periph_handle_t self)
{
    ESP_LOGI(TAG, "destroy");
    periph_mywifi_t *periph_mywifi = esp_periph_get_data(self);
    audio_free(periph_mywifi);
    return ESP_OK;
}

esp_periph_handle_t periph_mywifi_init(periph_mywifi_cfg_t *config)
{
    ESP_LOGI(TAG, "periph_mywifi_init");

    esp_periph_handle_t periph = esp_periph_create(PERIPH_ID_MYWIFI, "periph_mywifi");
    AUDIO_MEM_CHECK(TAG, periph, return NULL);
    periph_mywifi_t *periph_mywifi = audio_calloc(1, sizeof(periph_mywifi_t));

    AUDIO_MEM_CHECK(TAG, periph_mywifi, {
        audio_free(periph);
        return NULL;
    });
    esp_periph_set_data(periph, periph_mywifi);
    esp_periph_set_function(periph, _mywifi_init, _mywifi_run, _mywifi_destroy);

    return periph;
}
