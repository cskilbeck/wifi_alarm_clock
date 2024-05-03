#pragma once

#include "esp_peripherals.h"

#ifdef __cplusplus
extern "C" {
#endif

// Hmm - how to get the max defined PERIPH_ID ? Just hard code PERIPH_ID_LCD for now...
#define PERIPH_ID_MYWIFI (PERIPH_ID_LCD + 2)

/**
 * @brief      mywifi event id
 */

typedef enum
{
    PERIPH_MYWIFI_UNCHANGE = 0, /*!< No event */
    PERIPH_MYWIFI_SHOW_QR_CODE,
    PERIPH_MYWIFI_CONNECTED,
    PERIPH_MYWIFI_DISCONNECTED,    // etc

} mywifi_event_id_t;

/**
 * @brief      Create the mywifi peripheral handle for esp_peripherals.
 *
 * @note       The handle was created by this function automatically destroy when `esp_periph_destroy` is called
 *
 * @param      mywifi_cfg  The encoder configuration
 *
 * @return     The esp peripheral handle
 */
esp_periph_handle_t periph_mywifi_init();

#ifdef __cplusplus
}
#endif
