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

#ifndef __PERIPH_MYWIFI_H
#define __PERIPH_MYWIFI_H

#include "esp_peripherals.h"
#include "mywifi.h"
#include "../../../managed_components/espressif__qrcode/include/qrcode.h"

#ifdef __cplusplus
extern "C" {
#endif

// Hmm - how to get the max defined PERIPH_ID ? Just hard code PERIPH_ID_LCD for now...
#define PERIPH_ID_MYWIFI (PERIPH_ID_LCD + 2)

/**
 * @brief   mywifi peripheral configuration
 */
typedef struct
{
    mywifi_show_qr_code on_show_qr_code;

} periph_mywifi_cfg_t;


/**
 * @brief      Create the mywifi peripheral handle for esp_peripherals.
 *
 * @note       The handle was created by this function automatically destroy when `esp_periph_destroy` is called
 *
 * @param      mywifi_cfg  The encoder configuration
 *
 * @return     The esp peripheral handle
 */
esp_periph_handle_t periph_mywifi_init(periph_mywifi_cfg_t *mywifi_cfg);

#ifdef __cplusplus
}
#endif

#endif    //__PERIPH_MYWIFI_H
