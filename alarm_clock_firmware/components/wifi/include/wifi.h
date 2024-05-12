#pragma once

#include <esp_err.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define WIFI_CONNECTED_EVENT 1
#define WIFI_DISCONNECTED_EVENT 2
#define WIFI_ANY_BIT 3

extern EventGroupHandle_t wifi_event_group;

esp_err_t wifi_init();
esp_err_t wifi_factory_reset();


#if defined(__cplusplus)
}
#endif
