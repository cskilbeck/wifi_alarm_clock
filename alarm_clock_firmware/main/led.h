#pragma once

#include <esp_err.h>

#if defined(__cplusplus)
extern "C" {
#endif

esp_err_t led_init(void);
void led_set(bool on_or_off);
void led_set_on();
void led_set_off();
bool led_is_on(void);
void led_toggle(void);

#if defined(__cplusplus)
}
#endif
