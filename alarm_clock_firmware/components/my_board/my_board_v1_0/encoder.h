#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct encoder_msg_struct
{
    int direction;

} encoder_msg_t;

typedef void (*encoder_isr)(void *);

typedef struct encoder_config
{
    gpio_num_t gpio_a;
    gpio_num_t gpio_b;
    encoder_isr encoder_intr_handler;
    void *intr_context;
} encoder_config_t;

struct esp_encoder
{
    gpio_num_t gpio_a;
    gpio_num_t gpio_b;
    uint8_t state;
    uint8_t store;
};

typedef struct esp_encoder *esp_encoder_handle_t;

esp_err_t encoder_init(encoder_config_t *cfg, esp_encoder_handle_t *encoder);
esp_err_t encoder_destroy(esp_encoder_handle_t encoder);

int encoder_read(esp_encoder_handle_t encoder, int bits);

#if defined(__cplusplus)
}
#endif
