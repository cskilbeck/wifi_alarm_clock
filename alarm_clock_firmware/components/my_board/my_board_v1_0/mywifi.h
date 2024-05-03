#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include "../../../managed_components/espressif__qrcode/include/qrcode.h"

#if defined(__cplusplus)
extern "C" {
#endif

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

typedef struct mywifi_msg_struct
{
    int foo;

} mywifi_msg_t;

typedef void (*mywifi_show_qr_code)(esp_qrcode_handle_t qrcode_handle);

typedef struct mywifi_config
{
    mywifi_show_qr_code on_show_qr_code;

} mywifi_config_t;

typedef struct esp_mywifi *esp_mywifi_handle_t;

esp_err_t mywifi_init(mywifi_config_t *cfg, esp_mywifi_handle_t *mywifi);
esp_err_t mywifi_destroy(esp_mywifi_handle_t mywifi);

#if defined(__cplusplus)
}
#endif
