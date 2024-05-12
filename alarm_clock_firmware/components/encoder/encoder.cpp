//////////////////////////////////////////////////////////////////////

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"

#include "util.h"
#include "encoder.h"
#include "led.h"

//////////////////////////////////////////////////////////////////////

LOG_CONTEXT("encoder");

//////////////////////////////////////////////////////////////////////

struct encoder
{
    uint8_t encoder_state;
    uint8_t encoder_store;
    uint8_t button_history;
    uint8_t button_state;
    esp_timer_handle_t timer_handle;
    QueueHandle_t input_queue;
    encoder_config_t config;
};

//////////////////////////////////////////////////////////////////////

namespace
{
    uint8_t const encoder_valid_bits[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

    //////////////////////////////////////////////////////////////////////

    void IRAM_ATTR button_on_timer(void *e)
    {
        encoder_handle_t encoder = reinterpret_cast<encoder_handle_t>(e);
        int b = 1 - gpio_get_level(encoder->config.gpio_button);
        encoder->button_state = b;
        int cur_state = encoder->button_history;
        cur_state <<= 1;
        cur_state |= b;
        cur_state &= 3;
        encoder->button_history = cur_state;
        encoder_message_t msg = {};
        BaseType_t woken;
        if(cur_state == 2) {
            msg = ENCODER_MSG_RELEASE;
            xQueueSendFromISR(encoder->input_queue, &msg, &woken);
        } else if(cur_state == 1) {
            msg = ENCODER_MSG_PRESS;
            xQueueSendFromISR(encoder->input_queue, &msg, &woken);
        }
        portYIELD_FROM_ISR(woken);
    }

    //////////////////////////////////////////////////////////////////////

    void IRAM_ATTR encoder_isr_handler(void *e)
    {
        encoder_handle_t encoder = reinterpret_cast<encoder_handle_t>(e);
        int a = gpio_get_level(encoder->config.gpio_a);
        int b = gpio_get_level(encoder->config.gpio_b);
        int bits = (a << 1) | b;
        encoder->encoder_state <<= 2;
        encoder->encoder_state |= bits;
        encoder->encoder_state &= 0xf;

        BaseType_t woken = pdFALSE;

        encoder_message_t msg = ENCODER_MSG_NULL;

        if(encoder_valid_bits[encoder->encoder_state] != 0) {
            encoder->encoder_store = (encoder->encoder_store << 4) | encoder->encoder_state;
            switch((encoder->encoder_store)) {
            case 0xe8:
                msg = ENCODER_MSG_ROTATE_CW;
                break;
            case 0x2b:
                msg = ENCODER_MSG_ROTATE_CCW;
                break;
            }
        }
        if(msg != ENCODER_MSG_NULL) {
            xQueueSendFromISR(encoder->input_queue, &msg, &woken);
        }
        portYIELD_FROM_ISR(woken);
    }

}    // namespace

//////////////////////////////////////////////////////////////////////

esp_err_t encoder_init(encoder_config_t *cfg, encoder_handle_t *handle)
{
    LOG_I("init");

    encoder_handle_t encoder = (encoder_handle_t)heap_caps_calloc(1, sizeof(struct encoder), MALLOC_CAP_INTERNAL);

    encoder->config = *cfg;

    encoder->input_queue = xQueueCreate(16, sizeof(encoder_message_t));

    gpio_config_t gpiocfg = {
        .pin_bit_mask = (1llu << cfg->gpio_a) | (1llu << cfg->gpio_b),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&gpiocfg);

    gpiocfg.intr_type = GPIO_INTR_DISABLE;
    gpiocfg.pin_bit_mask = 1llu << cfg->gpio_button;
    gpio_config(&gpiocfg);

    LOG_I("Adding GPIO interrupt handlers");

    gpio_install_isr_service(ESP_INTR_CPU_AFFINITY_AUTO);

    gpio_set_intr_type(cfg->gpio_a, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(cfg->gpio_b, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(cfg->gpio_a, encoder_isr_handler, encoder);
    gpio_isr_handler_add(cfg->gpio_b, encoder_isr_handler, encoder);
    gpio_intr_enable(cfg->gpio_a);
    gpio_intr_enable(cfg->gpio_b);

    LOG_I("starting button timer");

    esp_timer_create_args_t button_timer_args = {};
    button_timer_args.callback = button_on_timer;
    button_timer_args.dispatch_method = ESP_TIMER_TASK;
    button_timer_args.skip_unhandled_events = false;
    button_timer_args.arg = encoder;
    ESP_ERROR_CHECK(esp_timer_create(&button_timer_args, &encoder->timer_handle));
    esp_timer_start_periodic(encoder->timer_handle, 1000000 / 100);

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

//////////////////////////////////////////////////////////////////////

esp_err_t encoder_get_message(encoder_handle_t encoder, encoder_message_t *msg)
{
    if(xQueueReceive(encoder->input_queue, msg, 0)) {
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}
