#pragma once

#include <esp_err.h>

#if defined(__cplusplus)
extern "C" {
#endif

esp_err_t stream_init(void);
esp_err_t stream_play(char const *url);
esp_err_t stream_stop();

#if defined(__cplusplus)
}
#endif
