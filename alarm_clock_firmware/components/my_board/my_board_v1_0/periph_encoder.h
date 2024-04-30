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

#ifndef __PERIPH_ENCODER_H
#define __PERIPH_ENCODER_H

#include "esp_peripherals.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Hmm - how to get the max defined PERIPH_ID ? Just hard code PERIPH_ID_LCD for now...
#define PERIPH_ID_ENCODER (PERIPH_ID_LCD + 1)

/**
 * @brief   The Encoder peripheral configuration
 */
typedef struct
{
    gpio_num_t gpio_num_a;
    gpio_num_t gpio_num_b;
} periph_encoder_cfg_t;

/**
 * @brief      Peripheral encoder event id
 */
typedef enum
{
    PERIPH_ENCODER_UNCHANGE = 0,      /*!< No event */
    PERIPH_ENCODER_CLOCKWISE,         /*! rotate cw */
    PERIPH_ENCODER_COUNTER_CLOCKWISE, /*! rotate ccw */

} periph_encoder_event_id_t;

/**
 * @brief      Create the encoder peripheral handle for esp_peripherals.
 *
 * @note       The handle was created by this function automatically destroy when `esp_periph_destroy` is called
 *
 * @param      encoder_cfg  The encoder configuration
 *
 * @return     The esp peripheral handle
 */
esp_periph_handle_t periph_encoder_init(periph_encoder_cfg_t *encoder_cfg);

#ifdef __cplusplus
}
#endif

#endif    //__PERIPH_ENCODER_H
