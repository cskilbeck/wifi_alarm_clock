#pragma once

#include <stdint.h>
#include <esp_err.h>
#include "util.h"
#include "display.h"

#if defined(__cplusplus)
extern "C" {
#endif

esp_err_t ui_init();

struct ui_element
{
    void *data;
    void *on_draw;
    struct ui_element *next;
};

#if defined(__cplusplus)
}
#endif
