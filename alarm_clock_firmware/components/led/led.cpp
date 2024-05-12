//////////////////////////////////////////////////////////////////////

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "soc/gpio_periph.h"
#include "esp_rom_gpio.h"

#include "util.h"
#include "led.h"

//////////////////////////////////////////////////////////////////////

LOG_CONTEXT("led");

#define LED_PIN ((gpio_num_t)7)

//////////////////////////////////////////////////////////////////////

namespace
{
    bool led_state;
}

//////////////////////////////////////////////////////////////////////

esp_err_t led_init()
{
    LOG_I("init");

    esp_rom_gpio_pad_select_gpio(LED_PIN);

    ESP_ERROR_CHECK(gpio_set_level(LED_PIN, 1));    // off

    ESP_ERROR_CHECK(gpio_set_pull_mode(LED_PIN, GPIO_FLOATING));
    ESP_ERROR_CHECK(gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_intr_type(LED_PIN, GPIO_INTR_DISABLE));
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

void led_set(bool on_or_off)
{
    led_state = on_or_off;
    gpio_set_level(LED_PIN, !led_state);
}

//////////////////////////////////////////////////////////////////////

void led_set_on()
{
    led_set(true);
}

//////////////////////////////////////////////////////////////////////

void led_set_off()
{
    led_set(false);
}

//////////////////////////////////////////////////////////////////////

void led_toggle()
{
    return led_set(!led_is_on());
}

//////////////////////////////////////////////////////////////////////

bool led_is_on()
{
    return led_state;
}
