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
#include "util.h"
#include "encoder.h"
#include "periph_encoder.h"
#include "audio_mem.h"

LOG_CONTEXT("PERIPH_ENCODER");

#define VALIDATE_ENCODER(periph, ret)                                 \
    if(!(periph && esp_periph_get_id(periph) == PERIPH_ID_ENCODER)) { \
        LOG_E("Invalid ENCODER periph, at line %d", __LINE__);        \
        return ret;                                                   \
    }

typedef struct
{
    esp_encoder_handle_t encoder;
    gpio_num_t gpio_a;
    gpio_num_t gpio_b;

} periph_encoder_t;

static void IRAM_ATTR encoder_intr_handler(void *param)
{
    esp_periph_handle_t periph = (esp_periph_handle_t)param;
    periph_encoder_t *periph_encoder = esp_periph_get_data(periph);
    esp_encoder_handle_t encoder = periph_encoder->encoder;
    int a = gpio_get_level(encoder->gpio_a);
    int b = gpio_get_level(encoder->gpio_b);
    esp_periph_send_cmd_from_isr(periph, 0, (void *)((a << 1) | b), 0);
}

static esp_err_t _encoder_run(esp_periph_handle_t self, audio_event_iface_msg_t *msg)
{
    periph_encoder_t *periph_encoder = esp_periph_get_data(self);
    int direction = encoder_read(periph_encoder->encoder, (int)msg->data);
    if(direction == -1) {
        return esp_periph_send_event(self, PERIPH_ENCODER_COUNTER_CLOCKWISE, NULL, 0);
    }
    if(direction == 1) {
        return esp_periph_send_event(self, PERIPH_ENCODER_CLOCKWISE, NULL, 0);
    }
    return ESP_OK;
}

static esp_err_t _encoder_destroy(esp_periph_handle_t self)
{
    LOG_I("destroy");
    periph_encoder_t *periph_encoder = esp_periph_get_data(self);
    encoder_destroy(periph_encoder->encoder);
    audio_free(periph_encoder);
    return ESP_OK;
}

static esp_err_t _encoder_init(esp_periph_handle_t self)
{
    LOG_I("init");

    VALIDATE_ENCODER(self, ESP_FAIL);
    periph_encoder_t *periph_encoder = esp_periph_get_data(self);

    encoder_config_t encoder_config = {
        .gpio_a = periph_encoder->gpio_a,
        .gpio_b = periph_encoder->gpio_b,
        .encoder_intr_handler = encoder_intr_handler,
        .intr_context = self,
    };
    return encoder_init(&encoder_config, &periph_encoder->encoder);
}

esp_periph_handle_t periph_encoder_init(periph_encoder_cfg_t *config)
{
    LOG_I("periph_encoder_init");

    esp_periph_handle_t periph = esp_periph_create(PERIPH_ID_ENCODER, "periph_encoder");
    AUDIO_MEM_CHECK(LOG_TAG, periph, return NULL);
    periph_encoder_t *periph_encoder = audio_calloc(1, sizeof(periph_encoder_t));

    AUDIO_MEM_CHECK(LOG_TAG, periph_encoder, {
        audio_free(periph);
        return NULL;
    });
    periph_encoder->gpio_a = config->gpio_num_a;
    periph_encoder->gpio_b = config->gpio_num_b;

    esp_periph_set_data(periph, periph_encoder);
    esp_periph_set_function(periph, _encoder_init, _encoder_run, _encoder_destroy);

    return periph;
}
