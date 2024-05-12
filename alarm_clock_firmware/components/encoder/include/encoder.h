//////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <esp_err.h>
#include <driver/gpio.h>
#include "util.h"

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

typedef enum encoder_msg
{
    ENCODER_MSG_NULL = 0,
    ENCODER_MSG_ROTATE_CW,
    ENCODER_MSG_ROTATE_CCW,
    ENCODER_MSG_PRESS,
    ENCODER_MSG_RELEASE

} encoder_message_t;

//////////////////////////////////////////////////////////////////////

typedef struct encoder_config
{
    gpio_num_t gpio_a;
    gpio_num_t gpio_b;
    gpio_num_t gpio_button;

} encoder_config_t;

struct encoder;

typedef struct encoder *encoder_handle_t;

//////////////////////////////////////////////////////////////////////

esp_err_t encoder_init(encoder_config_t *config, encoder_handle_t *handle);
esp_err_t encoder_destroy(encoder_handle_t encoder);

esp_err_t encoder_get_message(encoder_handle_t encoder, encoder_message_t *msg);

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif
