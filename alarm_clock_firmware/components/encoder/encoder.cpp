//////////////////////////////////////////////////////////////////////

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"

#include "util.h"
#include "encoder.h"

//////////////////////////////////////////////////////////////////////

LOG_CONTEXT("encoder");

//////////////////////////////////////////////////////////////////////

struct encoder
{
    gpio_num_t gpio_a;
    gpio_num_t gpio_b;
    uint8_t state;
    uint8_t store;
};

namespace
{

    uint8_t const encoder_valid_bits[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

}    // namespace

//////////////////////////////////////////////////////////////////////

int IRAM_ATTR encoder_read(encoder_handle_t encoder, int bits)
{
    encoder->state <<= 2;
    encoder->state |= bits;
    encoder->state &= 0xf;

    if(encoder_valid_bits[encoder->state] != 0) {
        encoder->store = (encoder->store << 4) | encoder->state;
        switch((encoder->store)) {
        case 0xe8:
            return 1;
        case 0x2b:
            return -1;
        }
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////

void encoder_isr_handler(void *e)
{
    encoder_handle_t encoder = reinterpret_cast<encoder_handle_t>(e);
    (void)encoder;
}

//////////////////////////////////////////////////////////////////////

esp_err_t encoder_init(encoder_config_t *cfg, encoder_handle_t *handle)
{
    LOG_I("init");

    encoder_handle_t encoder = (encoder_handle_t)heap_caps_calloc(1, sizeof(struct encoder), MALLOC_CAP_INTERNAL);

    encoder->gpio_a = cfg->gpio_a;
    encoder->gpio_b = cfg->gpio_b;

    gpio_config_t gpiocfg = {
        .pin_bit_mask = (1llu << cfg->gpio_a) | (1llu << cfg->gpio_b),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&gpiocfg);

    LOG_I("Adding GPIO interrupt handlers");
    gpio_set_intr_type(cfg->gpio_a, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(cfg->gpio_b, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(cfg->gpio_a, encoder_isr_handler, encoder);
    gpio_isr_handler_add(cfg->gpio_b, encoder_isr_handler, encoder);
    gpio_intr_enable(cfg->gpio_a);
    gpio_intr_enable(cfg->gpio_b);

    LOG_I("init done");

    *handle = encoder;

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t encoder_destroy(encoder_handle_t encoder)
{
    LOG_I("destroy");
    return ESP_OK;
}
