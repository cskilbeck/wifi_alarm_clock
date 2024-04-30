//////////////////////////////////////////////////////////////////////

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "audio_mem.h"

#include "encoder.h"

//////////////////////////////////////////////////////////////////////

static char const *TAG = "encoder";

//////////////////////////////////////////////////////////////////////

namespace
{
    uint8_t const encoder_valid_bits[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

}    // namespace

//////////////////////////////////////////////////////////////////////

int IRAM_ATTR encoder_read(esp_encoder_handle_t encoder, int bits)
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

esp_err_t encoder_init(encoder_config_t *cfg, esp_encoder_handle_t *handle)
{
    ESP_LOGI(TAG, "init");

    esp_encoder_handle_t encoder = (esp_encoder_handle_t)audio_calloc(1, sizeof(struct esp_encoder));

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

    if(cfg->encoder_intr_handler) {
        ESP_LOGI(TAG, "Adding GPIO interrupt handlers");
        gpio_set_intr_type(cfg->gpio_a, GPIO_INTR_ANYEDGE);
        gpio_set_intr_type(cfg->gpio_b, GPIO_INTR_ANYEDGE);
        gpio_isr_handler_add(cfg->gpio_a, cfg->encoder_intr_handler, cfg->intr_context);
        gpio_isr_handler_add(cfg->gpio_b, cfg->encoder_intr_handler, cfg->intr_context);
        gpio_intr_enable(cfg->gpio_a);
        gpio_intr_enable(cfg->gpio_b);
    }
    ESP_LOGI(TAG, "init done");

    *handle = encoder;

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t encoder_destroy(esp_encoder_handle_t encoder)
{
    ESP_LOGI(TAG, "destroy");
    audio_free(encoder);
    return ESP_OK;
}
